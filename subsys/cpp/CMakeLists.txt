# SPDX-License-Identifier: Apache-2.0

zephyr_sources(
  cpp_init_array.c
  cpp_ctors.c
  cpp_dtors.c
)

zephyr_sources_ifdef(NOT CONFIG_LIB_CPLUSPLUS
  cpp_virtual.c
  cpp_vtable.cpp
  cpp_new.cpp
)
