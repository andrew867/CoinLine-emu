# SPDX-License-Identifier: GPL-2.0-or-later
#
# CoinLine Terminal Emulator — CMake build for shared bring-up utilities and C++ tests.
# The MAME machine driver sources under src/mame/coinline/ are compiled by an external
# MAME tree (see scripts/register-millennium-driver.sh and third-party/README.md).

cmake_minimum_required(VERSION 3.20)
project(coinline_terminal_emulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(coinline_support STATIC
	src/mame/coinline/millennium_sha256.cpp
	src/mame/coinline/millennium_io_shared.cpp
	src/mame/coinline/millennium_voiceware_phrase_lookup.cpp
	src/mame/coinline/millennium_voiceware_phrase_port.cpp
	src/mame/coinline/millennium_firmware.cpp
	src/mame/coinline/millennium_debug.cpp
	src/mame/coinline/millennium_board_profile.cpp
	src/mame/coinline/millennium_board_hwinit.cpp
	src/mame/coinline/millennium_nvram_model.cpp
	src/mame/coinline/millennium_microwire_93c66.cpp
	src/mame/coinline/millennium_mach_pio.cpp
	src/mame/coinline/millennium_z180_snapshot.cpp
	src/mame/coinline/millennium_evidence_bundle.cpp
	src/mame/coinline/millennium_modem_ncc.cpp
	src/mame/coinline/millennium_terminal_peripherals_model.cpp
	src/mame/coinline/millennium_coin_model.cpp
	src/mame/coinline/millennium_card_model.cpp
	src/mame/coinline/millennium_smartcard_model.cpp
	src/mame/coinline/millennium_z180_boot_init_model.cpp
	src/mame/coinline/millennium_rtos_startup_model.cpp
	src/mame/coinline/millennium_voice_synthesis_model.cpp
	src/mame/coinline/millennium_user_io_overlay_model.cpp
	src/mame/coinline/millennium_supervision_fsm.cpp
	src/mame/coinline/millennium_vfd_profile_model.cpp
	src/mame/coinline/millennium_vfd_model.cpp
	src/mame/coinline/millennium_am8048_core.cpp
	src/mame/coinline/millennium_pcd3349a_contract.cpp
	src/mame/coinline/millennium_audio_model.cpp
)
target_include_directories(coinline_support PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/src/mame/coinline
)

enable_testing()

function(coinline_add_test exe_name label)
	add_executable(${exe_name} ${ARGN})
	target_link_libraries(${exe_name} PRIVATE coinline_support)
	target_compile_definitions(${exe_name} PRIVATE COINLINE_EMU_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
	add_test(NAME ${exe_name} COMMAND ${exe_name})
	set_tests_properties(${exe_name} PROPERTIES LABELS "${label}")
endfunction()

coinline_add_test(test_firmware_load boot tests/boot/test_firmware_load.cpp)
set_tests_properties(test_firmware_load PROPERTIES SKIP_RETURN_CODE 77)

coinline_add_test(test_firmware_hash_mismatch boot tests/boot/test_firmware_hash_mismatch.cpp)
coinline_add_test(test_firmware_size_mismatch boot tests/boot/test_firmware_size_mismatch.cpp)
coinline_add_test(test_reset_vector boot tests/boot/test_reset_vector.cpp)
coinline_add_test(test_first_instructions boot tests/boot/test_first_instructions.cpp)
coinline_add_test(test_ram_init boot tests/boot/test_ram_init.cpp)

coinline_add_test(test_unknown_port_logging unknown_port tests/devices/test_unknown_port_logging.cpp)
coinline_add_test(test_io_port_defaults unknown_port tests/devices/test_io_port_defaults.cpp)

coinline_add_test(test_z180_icr z180 tests/devices/test_z180_icr.cpp)
coinline_add_test(test_z180_mmu z180 tests/devices/test_z180_mmu.cpp)
coinline_add_test(test_z180_asci z180 tests/devices/test_z180_asci.cpp)
coinline_add_test(test_z180_prt z180 tests/devices/test_z180_prt.cpp)
coinline_add_test(test_z180_int z180 tests/devices/test_z180_int.cpp)
coinline_add_test(test_z180_dma z180 tests/devices/test_z180_dma.cpp)
coinline_add_test(test_z180_refresh z180 tests/devices/test_z180_refresh.cpp)
coinline_add_test(test_z180_wait_states z180 tests/devices/test_z180_wait_states.cpp)
coinline_add_test(test_z180_internal_register_readback z180 tests/devices/test_z180_internal_register_readback.cpp)
coinline_add_test(test_z180_mmu_register_readback z180 tests/devices/test_z180_mmu_register_readback.cpp)
coinline_add_test(test_z180_itc_iocr_readback z180 tests/devices/test_z180_itc_iocr_readback.cpp)

coinline_add_test(test_external_uart_rx_ready telephony tests/devices/test_external_uart_rx_ready.cpp)
coinline_add_test(test_external_uart_interrupts telephony tests/devices/test_external_uart_interrupts.cpp)
coinline_add_test(test_telephony_rx_path telephony tests/devices/test_telephony_rx_path.cpp)
coinline_add_test(test_telephony_ready_milestone telephony tests/devices/test_telephony_ready_milestone.cpp)
coinline_add_test(test_csio_cp_to_tp_qualifier telephony tests/devices/test_csio_cp_to_tp_qualifier.cpp)
coinline_add_test(test_telephony_runtime_vectors telephony tests/devices/test_telephony_runtime_vectors.cpp)
coinline_add_test(test_telephony_boot_contract_vectors telephony tests/devices/test_telephony_boot_contract_vectors.cpp)
coinline_add_test(test_telephony_boot_code_selection telephony tests/devices/test_telephony_boot_code_selection.cpp)
coinline_add_test(test_modem_ncc_framing telephony tests/devices/test_modem_ncc_framing.cpp)
coinline_add_test(test_modem_ncc_session_vectors telephony tests/devices/test_modem_ncc_session_vectors.cpp)
coinline_add_test(test_terminal_peripherals_vectors telephony tests/devices/test_terminal_peripherals_vectors.cpp)
coinline_add_test(test_voiceware_runtime_vectors telephony tests/devices/test_voiceware_runtime_vectors.cpp)
coinline_add_test(test_coin_validator_protocol_vectors telephony tests/devices/test_coin_validator_protocol_vectors.cpp)
coinline_add_test(test_card_track2_decode_vectors telephony tests/devices/test_card_track2_decode_vectors.cpp)
coinline_add_test(test_smartcard_protocol_negotiation telephony tests/devices/test_smartcard_protocol_negotiation.cpp)
coinline_add_test(test_z180_boot_init_vectors telephony tests/devices/test_z180_boot_init_vectors.cpp)
coinline_add_test(test_rtos_startup_scheduler_vectors telephony tests/devices/test_rtos_startup_scheduler_vectors.cpp)
coinline_add_test(test_voice_synthesis_task_vectors telephony tests/devices/test_voice_synthesis_task_vectors.cpp)
coinline_add_test(test_user_io_overlay_arbitration_vectors telephony tests/devices/test_user_io_overlay_arbitration_vectors.cpp)
coinline_add_test(test_disconnect_supervision_timing_vectors telephony tests/devices/test_disconnect_supervision_timing_vectors.cpp)
coinline_add_test(test_vfd_11line_profile_vectors telephony tests/devices/test_vfd_11line_profile_vectors.cpp)
coinline_add_test(test_vfd_command_decode vfd tests/devices/test_vfd_command_decode.cpp)
coinline_add_test(test_vfd_buffer_snapshot vfd tests/devices/test_vfd_buffer_snapshot.cpp)
coinline_add_test(test_vfd_2line_idle vfd tests/devices/test_vfd_2line_idle.cpp)
coinline_add_test(test_vfd_11line_ad vfd tests/devices/test_vfd_11line_ad.cpp)
coinline_add_test(test_vfd_2line_parity "vfd-parity;vfd" tests/devices/test_vfd_2line_parity.cpp)
coinline_add_test(test_tp8048_core_conformance telephony tests/devices/test_tp8048_core_conformance.cpp)
coinline_add_test(test_nvram_all_zero_fixture boot tests/devices/test_nvram_all_zero_fixture.cpp)
coinline_add_test(test_microwire_93c66_write_sink_vectors boot tests/devices/test_microwire_93c66_write_sink_vectors.cpp)
coinline_add_test(test_coinline_env_truthy boot tests/devices/test_coinline_env_truthy.cpp)
coinline_add_test(test_voiceware_voice_rom_sha256_manifest boot tests/devices/test_voiceware_voice_rom_sha256_manifest.cpp)
set_tests_properties(test_voiceware_voice_rom_sha256_manifest PROPERTIES SKIP_RETURN_CODE 77)
coinline_add_test(test_voiceware_phrase_lookup boot tests/devices/test_voiceware_phrase_lookup.cpp)
coinline_add_test(test_voiceware_phrase_port boot tests/devices/test_voiceware_phrase_port.cpp)
# Full-machine voiceware parity (T1–T2): run MAME with COINLINE_VOICEWARE_UPD7759_CORE=1 per HW-UPD7759-VOICE-parity-tests.md.
coinline_add_test(test_mach_pio_c0 boot tests/devices/test_mach_pio_c0.cpp)
coinline_add_test(test_board_hwinit_shadow boot tests/devices/test_board_hwinit_shadow.cpp)
coinline_add_test(test_mach_async_masks boot tests/devices/test_mach_async_masks.cpp)
coinline_add_test(test_mach_port_f_square boot tests/devices/test_mach_port_f_square.cpp)

set(COINLINE_TOOLS_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tools/common)
set(COINLINE_MAME_COINLINE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/mame/coinline)

add_library(evidence_export_client STATIC tools/evidence-bundle-export/evidence_export_client.cpp)
target_include_directories(evidence_export_client PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/tools/evidence-bundle-export
	${COINLINE_MAME_COINLINE_DIR}
	${COINLINE_TOOLS_COMMON_DIR})
target_link_libraries(evidence_export_client PUBLIC coinline_support)
if(WIN32)
	target_link_libraries(evidence_export_client PUBLIC ws2_32)
endif()
target_compile_features(evidence_export_client PUBLIC cxx_std_20)

add_executable(evidence-bundle-export tools/evidence-bundle-export/main.cpp)
target_link_libraries(evidence-bundle-export PRIVATE evidence_export_client)
target_compile_features(evidence-bundle-export PRIVATE cxx_std_20)

add_executable(test_evidence_bundle_export_tool tools/evidence-bundle-export/tests/test_export.cpp)
target_link_libraries(test_evidence_bundle_export_tool PRIVATE evidence_export_client)
target_compile_definitions(test_evidence_bundle_export_tool PRIVATE COINLINE_EMU_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME test_evidence_bundle_export_tool COMMAND test_evidence_bundle_export_tool)
set_tests_properties(test_evidence_bundle_export_tool PROPERTIES LABELS "tool")

function(coinline_add_integration_test exe_name label)
	add_executable(${exe_name} ${ARGN})
	target_link_libraries(${exe_name} PRIVATE coinline_support)
	target_compile_definitions(${exe_name} PRIVATE COINLINE_EMU_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
	target_include_directories(${exe_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration)
	add_test(NAME ${exe_name} COMMAND ${exe_name})
	set_tests_properties(${exe_name} PROPERTIES LABELS "${label}")
endfunction()

coinline_add_integration_test(test_evidence_bundle evidence_bundle tests/integration/test_evidence_bundle.cpp)
coinline_add_integration_test(test_terminal_full_stack_vectors evidence_bundle tests/integration/test_terminal_full_stack_vectors.cpp)

message(STATUS "coinline-emu: configured C++ tests (labels: boot, unknown_port, z180, telephony, vfd, vfd-parity, evidence_bundle, tool). "
	"MAME driver is built separately via EXTERNAL_MAME_ROOT.")
