#ifndef _SMI130_ACCEL_H_
#define _SMI130_ACCEL_H_

struct regmap;

enum {
	smi130,
};

int smi130_accel_core_probe(struct device *dev, struct regmap *regmap, const char *name);
int smi130_accel_core_remove(struct device *dev);
extern const struct regmap_config smi130_regmap_conf;

#endif  /* _SMI130_ACCEL_H_ */
