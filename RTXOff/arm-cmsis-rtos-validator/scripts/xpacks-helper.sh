#!/bin/bash
#set -euo pipefail
#IFS=$'\n\t'

# -----------------------------------------------------------------------------
# Bash helper script used in project generate.sh scripts.
# -----------------------------------------------------------------------------

# Optional args: src folders, like posix-io, driver
do_add_arm_cmsis_rtos_validator_xpack() {
  local pack_name='arm-cmsis-rtos-validator'
  do_tell_xpack "${pack_name}-xpack"

  do_select_pack_folder "ilg/${pack_name}.git"

  do_prepare_dest "${pack_name}/arm/include"
  do_add_content "${pack_folder}/Include"/* 

  do_prepare_dest "${pack_name}/cmsis-plus/include"
  do_add_content "${pack_folder}/cmsis-plus/include"/*

  do_prepare_dest "${pack_name}/cmsis-rtos-valid/include"
  do_add_content "${pack_folder}/test/cmsis-rtos-valid"/*

  do_prepare_dest "${pack_name}/arm/src"
  do_add_content "${pack_folder}/Source/"* 

  do_prepare_dest "${pack_name}/cmsis-plus/src"
  do_add_content "${pack_folder}/cmsis-plus/src"/* 
}

# -----------------------------------------------------------------------------
