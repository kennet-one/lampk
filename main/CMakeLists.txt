# SPDX-License-Identifier: Apache-2.0

idf_component_register(SRCS "mesh_main.c"
                            "mesh_v2_link.c"
                            "lamp_node.c"
                            "lamp_schedule.c"
                            "legacy_proto.c"
                            "legacy_root_sender.c"
                    PRIV_REQUIRES keemash_mesh_core esp_wifi esp_driver_gpio nvs_flash
                                  esp_timer esp_system esp_app_format app_update
                                  spi_flash bootloader_support
                    INCLUDE_DIRS ".")
