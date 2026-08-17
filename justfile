idf_version := env_var_or_default("IDF_VERSION", "v6.0.2")
default_target := "esp32c6"
default_port := env_var_or_default("ESPPORT", "")
install_base := env_var_or_default("ESPRESSIF_INSTALL_BASE", "C:/esp")
python := "python"

default:
  @just --list

doctor:
  @{{python}} tools/eim_idf.py doctor --target "{{default_target}}" --version "{{idf_version}}" --port "{{default_port}}"

build:
  @{{python}} tools/eim_idf.py build --target "{{default_target}}" --version "{{idf_version}}"

flash port=default_port:
  @{{python}} tools/eim_idf.py flash --target "{{default_target}}" --version "{{idf_version}}" --port "{{port}}"

monitor port=default_port:
  @{{python}} tools/eim_idf.py monitor --target "{{default_target}}" --version "{{idf_version}}" --port "{{port}}"

flash-monitor port=default_port:
  @{{python}} tools/eim_idf.py flash-monitor --target "{{default_target}}" --version "{{idf_version}}" --port "{{port}}"

menuconfig:
  @{{python}} tools/eim_idf.py menuconfig --target "{{default_target}}" --version "{{idf_version}}"

set-target target=default_target:
  @{{python}} tools/eim_idf.py set-target --target "{{target}}" --version "{{idf_version}}"

clean:
  @{{python}} tools/eim_idf.py clean --target "{{default_target}}" --version "{{idf_version}}"

fullclean:
  @{{python}} tools/eim_idf.py fullclean --target "{{default_target}}" --version "{{idf_version}}"

erase-flash port=default_port:
  @{{python}} tools/eim_idf.py erase-flash --target "{{default_target}}" --version "{{idf_version}}" --port "{{port}}"

size:
  @{{python}} tools/eim_idf.py size --target "{{default_target}}" --version "{{idf_version}}"

eim-install version=idf_version target=default_target install_base=install_base:
  @{{python}} tools/eim_idf.py eim-install --target "{{target}}" --version "{{version}}" --install-base "{{install_base}}"

eim-list:
  @{{python}} tools/eim_idf.py eim-list

eim-fix target=default_target:
  @{{python}} tools/eim_idf.py eim-fix --target "{{target}}" --version "{{idf_version}}"

eim-select version:
  @{{python}} tools/eim_idf.py eim-select --version "{{version}}"
