#include <stdio.h>
#include <dlfcn.h>

int main(int argc, char* argv[], char** e) {
    printf("test dl %x\n",&argc);
    for(int i=0;i<argc;i++){
        printf("  argv[%d]=%s\n",i,argv[i]);
    }
    char **envp = argv+argc+1;
    printf(" envp=%x %x\n",envp,e);

    // void *lib_handle;
    // void (*my_function)(int a);

    // lib_handle = dlopen("./libtest-so.so", RTLD_LAZY);
    // if (!lib_handle) {
    //     fprintf(stderr, "Failed to open library: %s\n", dlerror());
    //     return 1;
    // }

    // my_function = dlsym(lib_handle, "foo");
    // if (!my_function) {
    //     fprintf(stderr, "Failed to get symbol: %s\n", dlerror());
    //     dlclose(lib_handle);
    //     return 1;
    // }



    // my_function(11);

    // dlclose(lib_handle);

    return 0;
}