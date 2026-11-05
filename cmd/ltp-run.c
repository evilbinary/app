// ltp-run —— 给 LTP 指定可写的临时目录再启动它。
// 起因：根目录下 mkdir("/tmp") 返回 EPERM（只有根节点这一层失败，
// /bin/testdir 之类都正常），而 LTP 的 tst_tmpdir 默认要 mkdtemp("/tmp/LTP_xxx")，
// 于是所有用例都 BROK。这里把 TMPDIR 指到 /bin（可写、可 mkdir）绕过去。
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
    char **av;
    int i;

    setenv("TMPDIR", "/bin", 1);

    av = (char **)malloc(sizeof(char *) * (argc + 1));
    if (av == NULL) {
        return 1;
    }
    av[0] = (char *)"/bin/ltp";
    for (i = 1; i < argc; i++) {
        av[i] = argv[i];
    }
    av[argc] = NULL;

    execv("/bin/ltp", av);
    printf("ltp-run: execv /bin/ltp failed\n");
    return 1;
}
