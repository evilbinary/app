/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_device-drivers_pci_tpci_user_tpci
#include "ltp/testcases/kernel/device-drivers/pci/tpci_user/tpci.c"
#undef main
int LTP_OLD_MAIN_device-drivers_pci_tpci_user_tpci();
int ltp_run_device-drivers_pci_tpci_user_tpci(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_device-drivers_pci_tpci_user_tpci)(argc, argv);
}
struct tst_test *ltp_get_device-drivers_pci_tpci_user_tpci(void);
struct tst_test *ltp_get_device-drivers_pci_tpci_user_tpci(void) { return (void *)0; }
