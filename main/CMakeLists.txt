idf_component_register(SRCS "classtable.c" "converter.c" "memman.c" "interpreter.c" "class.c" "monitor.c" "heap.c" "thread.c" "native_methods_service.c" "loader.c" "init.c" "stringpool.c" "bumper.c"
                            "parser.c" "main.c" "stream.c" "natives/java/lang/Object.c" "natives/java/lang/System.c" "natives/java/io/NativeOutputStream.c"
                           
                    INCLUDE_DIRS ".")
fatfs_create_spiflash_image(storage "java_src" FLASH_IN_PROJECT)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-value)