idf_component_register(
    SRCS "test_uart_rx.c"
    INCLUDE_DIRS "."
    REQUIRES
        uart_rx
        unity
)