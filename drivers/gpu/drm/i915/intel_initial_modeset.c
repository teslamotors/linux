/*
 *
 * Copyright (c) 2016 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions: *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * DOC: Boot-time mode setting.
 *
 * There exists a use case where the kernel graphics needs to be initialized
 * with a valid display configuration with full display pipeline programming
 * in place before user space is initialized and without a fbdev & fb console.
 *
 * The primary motivation is to allow early user space applications to
 * display a frame (or frames) as soon as possible after user space starts.
 * Eliminating the time it takes userspace to program the display configuration
 * benefits this use case.
 *
 * By doing all the display programming in the kernel, it can be done in
 * parallel with other kernel startup tasks without adding significant
 * elapshed time before user space starts.
 */

#include <linux/firmware.h>
#include "intel_drv.h"
#include "i915_drv.h"

static inline struct drm_encoder *get_encoder(struct drm_connector *connector)
{
	struct intel_encoder *encoder;

	encoder = intel_attached_encoder(connector);

	return &encoder->base;
}

/*
 * This makes use of the video= kernel command line to determine what
 * connectors to configure. See Documentation/fb/modedb.txt for details
 * on the format.  There are 3 specific cases that are used:
 *
 * 1) video=<connector>
 *      - assume monitor is connected, use EDID preferred mode
 * 2) video=<connector:e>
 *      - use regardless of monitor connected, use EDID prefered mode
 * 3) video=<connector:xres x yres @ refresh e
 *      - use regardless of monitor connected and use specified mode.
 */
static bool use_connector(struct drm_connector *connector)
{
	char *option = NULL;
	struct drm_cmdline_mode *cl_mode = &connector->cmdline_mode;

	fb_get_options(connector->name, &option);
	if (option) {
		switch(connector->force) {

			case DRM_FORCE_OFF :
				return false;
			case DRM_FORCE_ON :
			case DRM_FORCE_ON_DIGITAL:
				return true;
			case DRM_FORCE_UNSPECIFIED:
				break;
		}

		connector->status = connector->funcs->detect(connector, true);
		if (connector->status != connector_status_connected) {
			connector->force = cl_mode->force;
			connector->status = connector_status_connected;
		}
		return true;
	}

	return false;
}

static bool attach_crtc(struct drm_device *dev, struct drm_encoder *encoder,
			uint32_t *used_crtcs)
{
	struct drm_crtc *possible_crtc;

	if(encoder->crtc != NULL &&
	   !(*used_crtcs & drm_crtc_mask(encoder->crtc))) {
		*used_crtcs |= drm_crtc_mask(encoder->crtc);
		return true;
	}

	drm_for_each_crtc(possible_crtc, dev) {
		if (!(encoder->possible_crtcs & drm_crtc_mask(possible_crtc))
		    || (*used_crtcs & drm_crtc_mask(possible_crtc)))
			continue;
		*used_crtcs |= drm_crtc_mask(possible_crtc);
		encoder->crtc = possible_crtc;
		return true;
	}

	return false;
}

static struct drm_framebuffer *
intel_splash_screen_fb(struct drm_device *dev,
		       struct splash_screen_info *splash_info)
{
	struct drm_framebuffer *fb;
	struct drm_mode_fb_cmd2 mode_cmd = {0};

	if (splash_info->obj == NULL)
		return NULL;

	mode_cmd.width = splash_info->width;
	mode_cmd.height = splash_info->height;

	mode_cmd.pitches[0] = splash_info->pitch * 4;
	mode_cmd.pixel_format = DRM_FORMAT_ARGB8888;

	mutex_lock(&dev->struct_mutex);
	fb = intel_framebuffer_create(splash_info->obj, &mode_cmd);
	mutex_unlock(&dev->struct_mutex);

	return fb;
}

static bool shared_image(struct drm_i915_private *dev_priv,
			 char *image,
			 struct splash_screen_info *info)
{
	struct splash_screen_info *splash_info;

	list_for_each_entry(splash_info, &dev_priv->splash_list, link) {
		if (strcmp(splash_info->image_name, image) == 0) {
			info->image_name = NULL;
			info->fw = NULL;
			info->obj = splash_info->obj;
			return true;
		}
	}
	return false;
}

static struct splash_screen_info *match_splash_info(
					struct drm_i915_private *dev_priv,
					char* name)
{
	struct splash_screen_info *splash_info, *info = NULL;
	list_for_each_entry(splash_info, &dev_priv->splash_list, link) {
		if (strcmp(splash_info->connector_name, name) == 0)
			info = splash_info;
	}
	return info;
}

static void intel_splash_screen_init(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = to_i915(dev);
	struct splash_screen_info *splash_info = NULL;
	char *splash_dup = NULL;
	char *splash_str = NULL;
	char *sep;
	u32 fw_npages;
	char *splash = i915_modparams.splash;

	INIT_LIST_HEAD(&dev_priv->splash_list);

	if (!splash)
		return;

	splash_dup = kstrdup(splash, GFP_KERNEL);
	if (!splash_dup)
		goto fail;
	splash_str = splash_dup;

	/*
	 * The loop condition find the connector name portion of the
	 * string.  Once we have that, we parse the following fields
	 * from the string:
	 *  Image data file name, image data width, image data height,
	 *  crtc rectangle (x, y, w, h).
	 * Then the loop condition will execute again to get the next
	 * connector name.
	 */
	while ((sep = strchr(splash_str, ':'))) {
		splash_info = kzalloc(sizeof(struct splash_screen_info),
				      GFP_KERNEL);
		if (splash_info == NULL)
			goto fail;

		*sep = '\0';
		splash_info->connector_name = kstrdup(splash_str, GFP_KERNEL);
		if (!splash_info->connector_name)
			goto fail;
		splash_str = sep + 1;

		/*
		 * Pull firmware file name from string and check to see
		 * if this image has been previously loaded.  request
		 * firmware only needs to be called once for each file.
		 */
		sep = strchr(splash_str, ':');
		if (sep == NULL)
			goto fail;

		*sep = '\0';

		if (!shared_image(dev_priv, splash_str, splash_info)) {
			splash_info->image_name = kstrdup(splash_str,
							  GFP_KERNEL);
			if (!splash_info->image_name)
				goto fail;
			request_firmware(&splash_info->fw, splash_str,
					 &dev_priv->drm.pdev->dev);
			if (splash_info->fw == NULL)
				goto fail;
		}
		splash_str = sep + 1;

		/* Pull splash screen width, height, crtc */
		sscanf(splash_str, "%d,%d,%d,%d,%d,%d,%d",
					&splash_info->width,
					&splash_info->height,
					&splash_info->pitch,
					&splash_info->crtc_x,
					&splash_info->crtc_y,
					&splash_info->crtc_w,
					&splash_info->crtc_h);

		/* Only do this we haven't mapped this firmware image before */
		if (splash_info->fw) {
			/*
			 * If splash image is baked into the kernel, we just get
			 * a pointer.  Otherwise we'll get a list of pages.
			 */
			fw_npages = DIV_ROUND_UP_ULL(splash_info->fw->size,
						     PAGE_SIZE);
			if (splash_info->fw->pages == NULL)
				splash_info->obj = i915_gem_object_create_splash(
					dev_priv,
					splash_info->fw->data,
					fw_npages);
			else
				splash_info->obj = i915_gem_object_create_splash_pages(
					dev_priv,
					splash_info->fw->pages, fw_npages);
		}

		list_add_tail(&splash_info->link, &dev_priv->splash_list);

		/* move to the next entry, break if reaching the end */
		splash_str = strchr(splash_str, ':');
		if(splash_str != NULL)
			splash_str += 1;
		else
			break;
	}

	kfree(splash_dup);
	return;

fail:
	/* Clean up failed entry data */
	if (splash_info) {
		release_firmware(splash_info->fw);
		kfree(splash_info->connector_name);
		kfree(splash_info->image_name);
	}
	kfree(splash_info);
	kfree(splash_dup);
	return;
}

static struct drm_display_mode *get_modeline(struct drm_i915_private *dev_priv,
					     struct drm_connector *connector,
					     int width, int height)
{
	struct drm_display_mode *mode;
	struct drm_cmdline_mode *cl_mode = &connector->cmdline_mode;

	/*
	 * fill_modes() takes a bit of time but is necessary.
	 * It is reading the EDID (or loading the EDID firmware blob
	 * and building the connector mode list. The time can be
	 * minimized by using a small EDID blob built into the kernel.
	 */

	connector->funcs->fill_modes(connector, width, height);

	/*
	 * Search the mode list.  If a mode was specified using the
	 * video= command line, use that.  Otherwise look for the
	 * preferred mode.
	 *
	 * <connector:><xres>x<yres>[M][R][-<bpp>][@<refresh>][i][m][eDd]
	 */
	list_for_each_entry(mode, &connector->modes, head) {
		if (cl_mode && cl_mode->specified &&
		    cl_mode->refresh_specified) {
			if (mode->hdisplay == cl_mode->xres &&
			    mode->vdisplay == cl_mode->yres &&
			    mode->vrefresh == cl_mode->refresh)
				return mode;
		} else if (cl_mode && cl_mode->specified) {
			if (mode->hdisplay == cl_mode->xres &&
			    mode->vdisplay == cl_mode->yres)
				return mode;
		} else {
			if (mode->type & DRM_MODE_TYPE_PREFERRED)
				return mode;
		}
	}

	DRM_ERROR("Failed to find a valid mode.\n");
	return NULL;
}

static int update_crtc_state(struct drm_atomic_state *state,
			     struct drm_display_mode *mode,
			     struct drm_crtc *crtc)
{
	struct drm_crtc_state *crtc_state;
	struct drm_rgba bgcolor;
	unsigned int bg_color = i915_modparams.bg_color;
	int ret;

	crtc_state = drm_atomic_get_crtc_state(state, crtc);
	if (IS_ERR(crtc_state))
		return PTR_ERR(crtc_state);

	ret = drm_atomic_set_mode_for_crtc(crtc_state, mode);
	if (ret) {
		crtc_state->active = false;
		return ret;
	}

	crtc_state->active = true;

	if (!IS_GEN9(to_i915(state->dev)))
	    return 0;

	/* Set the background color based on module parameter */
	bgcolor =drm_rgba(8,
			  (bg_color & 0x000000ff),
			  (bg_color & 0x0000ff00) >> 8,
			  (bg_color & 0x00ff0000) >> 16,
			  (bg_color & 0xff000000) >> 24);

	ret = drm_atomic_crtc_set_property(crtc, crtc_state,
					   state->dev->mode_config.prop_background_color,
					   bgcolor.v);
	WARN_ON(ret);

	return 0;
}

static int update_connector_state(struct drm_atomic_state *state,
				  struct drm_connector *connector,
				  struct drm_crtc *crtc)
{
	struct drm_connector_state *conn_state;
	int ret;

	conn_state = drm_atomic_get_connector_state(state, connector);
	if (IS_ERR(conn_state)) {
		DRM_DEBUG_KMS("failed to get connector %s state\n",
			      connector->name);
		return PTR_ERR(conn_state);
	}

	ret = drm_atomic_set_crtc_for_connector(conn_state, crtc);
	if (ret) {
		DRM_DEBUG_KMS("failed to set crtc for connector\n");
		return ret;
	}

	return 0;
}

static int update_primary_plane_state(struct drm_atomic_state *state,
				      struct splash_screen_info *splash_info,
				      struct drm_crtc *crtc,
				      struct drm_display_mode *mode)
{
	int hdisplay, vdisplay;
	struct drm_plane_state *primary_state;
	int ret;

	primary_state = drm_atomic_get_plane_state(state, crtc->primary);
	ret = drm_atomic_set_crtc_for_plane(primary_state, crtc);
	if (ret)
		return ret;
	drm_mode_get_hv_timing(mode, &hdisplay, &vdisplay);
	drm_atomic_set_fb_for_plane(primary_state, splash_info->fb);

	primary_state->crtc_x = splash_info->crtc_x;
	primary_state->crtc_y = splash_info->crtc_y;
	primary_state->crtc_w = (splash_info->crtc_w) ?
		splash_info->crtc_w : hdisplay;
	primary_state->crtc_h = (splash_info->crtc_h) ?
		splash_info->crtc_h : vdisplay;

	primary_state->src_x = 0 << 16;
	primary_state->src_y = 0 << 16;
	primary_state->src_w = ((splash_info->width) ?
		splash_info->width : hdisplay) << 16;
	primary_state->src_h = ((splash_info->height) ?
		splash_info->height : vdisplay) << 16;
	primary_state->rotation = DRM_MODE_ROTATE_0;

	return 0;
}

static void create_splash_fb(struct drm_device *dev,
				struct splash_screen_info *splash)
{
	struct splash_screen_info *splash_info;
	struct drm_i915_private *dev_priv = dev->dev_private;

	splash->fb = intel_splash_screen_fb(dev, splash);
	if (IS_ERR(splash->fb))
		splash->fb = NULL;

	if (splash->fb)
		list_for_each_entry(splash_info, &dev_priv->splash_list, link)
			if (splash->obj == splash_info->obj &&
			    splash != splash_info) {
				splash_info->fb = splash->fb;
				drm_framebuffer_reference(splash_info->fb);
			}
}

static int update_atomic_state(struct drm_device *dev,
			       struct drm_atomic_state *state,
			       struct drm_connector *connector,
			       struct drm_display_mode *mode)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_crtc *crtc;
	int ret;
	struct splash_screen_info *splash_info;

	if (get_encoder(connector))
		crtc = get_encoder(connector)->crtc;
	else
		return -EINVAL;

	ret = update_crtc_state(state, mode, crtc);
	if (ret)
		return ret;

	/* attach connector to atomic state */
	ret = update_connector_state(state, connector, crtc);
	if (ret)
		return ret;

	/* set up primary plane if a splash screen is requested */
	splash_info = match_splash_info(dev_priv, connector->name);
	if (splash_info) {
		if (splash_info->fb == NULL)
			create_splash_fb(dev, splash_info);
		if (splash_info->fb) {
			ret = update_primary_plane_state(state,
							 splash_info,
							 crtc, mode);
			if (ret)
				return ret;
		}
	}

	return 0;
}


static int disable_planes(struct drm_device *dev,
             struct drm_atomic_state *state)
{
   struct drm_plane *plane;
   int ret;

   drm_for_each_plane(plane, dev) {
       struct drm_plane_state *plane_state;

       plane->old_fb = plane->fb;

       plane_state = drm_atomic_get_plane_state(state, plane);
       if (IS_ERR(plane_state)) {
           return PTR_ERR(plane_state);
       }

       ret = drm_atomic_set_crtc_for_plane(plane_state, NULL);
       if (ret != 0)
           return ret;

       drm_atomic_set_fb_for_plane(plane_state, NULL);
   }

   return 0;
}


/*
 * The modeset_config is scheduled to run via an async
 * schedule call from the main driver load.
 */
static void modeset_config_fn(struct work_struct *work)
{
	struct drm_i915_private *dev_priv =
		container_of(work, typeof(*dev_priv), initial_modeset_work);
	struct drm_device *dev = &dev_priv->drm;
	struct drm_connector *connector;
	struct drm_connector_list_iter conn_iter;
	struct drm_atomic_state *state;
	struct drm_modeset_acquire_ctx ctx;
	struct drm_plane *plane;
	int ret;
	bool found = false;
	uint32_t used_crtcs = 0;
	struct drm_display_mode *connector_mode[20];
	struct drm_encoder *encoder;
	struct drm_display_mode *mode;

	intel_splash_screen_init(dev);

	memset(connector_mode, 0, sizeof(connector_mode));
	mutex_lock(&dev->mode_config.mutex);
	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter) {
		if (use_connector(connector)) {
			if (!(encoder = get_encoder(connector)))
				continue;
			if (!attach_crtc(dev, encoder, &used_crtcs))
				continue;
			mode = get_modeline(dev_priv, connector,
					    dev->mode_config.max_width,
					    dev->mode_config.max_height);
			if (mode) {
				found = true;
				WARN_ON(connector->index >= 20);
				connector_mode[connector->index] = mode;
			}
		}
	}
	drm_connector_list_iter_end(&conn_iter);
	if (!found) {
		used_crtcs = 0;
		/* Try to detect attached connectors */
		drm_connector_list_iter_begin(dev, &conn_iter);
		drm_for_each_connector_iter(connector, &conn_iter) {
                        drm_modeset_lock(&dev->mode_config.connection_mutex, NULL);
                        if (connector->funcs && connector->funcs->detect)
                                connector->status = connector->funcs->detect(connector,
                                                                            true);
                        else if (connector->helper_private && connector->helper_private->detect_ctx)
                                connector->status = connector->helper_private->detect_ctx(connector,
                                                                            NULL, true);
                        drm_modeset_unlock(&dev->mode_config.connection_mutex);

			if (connector->status == connector_status_connected) {
				if (!(encoder = get_encoder(connector)))
					continue;
				if (!attach_crtc(dev, encoder, &used_crtcs))
					continue;
				mode = get_modeline(dev_priv, connector,
						    dev->mode_config.max_width,
						    dev->mode_config.max_height);
				if (mode) {
					found = true;
					WARN_ON(connector->index >= 20);
					connector_mode[connector->index] = mode;
				}
			}
		}
		drm_connector_list_iter_end(&conn_iter);
	}
	mutex_unlock(&dev->mode_config.mutex);

	if (!found)
		return;

	state = drm_atomic_state_alloc(dev);
	if (!state)
		return;

	mutex_lock(&dev->mode_config.mutex);

	drm_modeset_acquire_init(&ctx, 0);
	state->acquire_ctx = &ctx;
retry:
	ret = drm_modeset_lock_all_ctx(dev, &ctx);
	if (ret == -EDEADLK) {
		drm_modeset_backoff(&ctx);
		goto retry;
	} else if(ret) {
		goto out;
	}

	ret = disable_planes(dev, state);
	if (ret)
		goto fail;

	/*
	 * For each connector that we want to set up, update the atomic
	 * state to include the connector and crtc mode.
	 */
	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter) {
		if (connector_mode[connector->index]) {
			ret = update_atomic_state(dev, state, connector,
						  connector_mode[connector->index]);
			if (ret)
				goto fail;
		}
	}
	drm_connector_list_iter_end(&conn_iter);

	ret = drm_atomic_commit(state);
	if (ret)
		goto fail;
	goto out;

fail:
	if (ret == -EDEADLK) {
		DRM_DEBUG_KMS("modeset commit deadlock, retry...\n");
		drm_modeset_backoff(&ctx);
		drm_atomic_state_clear(state);
		goto retry;
	}

out:
	if (!ret) {
		drm_for_each_plane(plane, dev) {
			if (plane->old_fb)
				drm_framebuffer_unreference(plane->old_fb);
		}
	}
	drm_modeset_drop_locks(&ctx);
	drm_modeset_acquire_fini(&ctx);
	drm_atomic_state_put(state);

	mutex_unlock(&dev->mode_config.mutex);
}

void intel_initial_mode_config_init(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = to_i915(dev);

	INIT_WORK(&dev_priv->initial_modeset_work, modeset_config_fn);
	schedule_work(&dev_priv->initial_modeset_work);
}

static void initial_mode_destroy(struct drm_device *dev)
{
	struct drm_atomic_state *state;
	struct drm_modeset_acquire_ctx ctx;
	int ret;

	state = drm_atomic_state_alloc(dev);
	if (!state)
		return;

	drm_modeset_acquire_init(&ctx, 0);
	state->acquire_ctx = &ctx;
	drm_modeset_lock_all_ctx(dev, &ctx);

retry:
	ret = disable_planes(dev, state);
	if (ret == -EDEADLK) {
		drm_modeset_backoff(&ctx);
		drm_atomic_state_clear(state);
		goto retry;
	}

	ret = drm_atomic_commit(state);
	if (ret == -EDEADLK) {
		drm_modeset_backoff(&ctx);
		drm_atomic_state_clear(state);
		goto retry;
	}

	drm_modeset_drop_locks(&ctx);
	drm_modeset_acquire_fini(&ctx);
}

void intel_initial_mode_config_fini(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = to_i915(dev);
	struct splash_screen_info *splash_info, *tmp;

	flush_work(&dev_priv->initial_modeset_work);
	initial_mode_destroy(dev);

	list_for_each_entry_safe(splash_info, tmp,
				 &dev_priv->splash_list, link) {
		if (splash_info->fb)
			drm_framebuffer_unreference(splash_info->fb);
		release_firmware(splash_info->fw);
		kfree(splash_info->connector_name);
		kfree(splash_info->image_name);
		kfree(splash_info);
	}
}
