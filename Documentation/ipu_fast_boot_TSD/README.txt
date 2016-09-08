The file in this folder is used for optimize the isys probe latency for TSD
'fastboot' KPI

How to apply them?

0001-Remove-the-adv7481b_eval_crl_sd-from-platform-data-f.patch
1. cd  yocto_build/repo-ext/linux-yocto/
2. git am 0001-Remove-the-adv7481b_eval_crl_sd-from-platform-data-f.patch

kernel_ipu.cfg
Copy the "kernel_ipu.cfg" file into folder
yocto_build/meta-intel/common/recipes-kernel/linux/linux-yocto/ and override the
old one
