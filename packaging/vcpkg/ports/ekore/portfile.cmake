vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO vlonexti/EKore
    REF "v${VERSION}"
    SHA512 332ee5502d3370cc77c8a70c1cb2e4f43c0b78e28182cadf52af44c1245effd2fe946e167251f01ef35b6c3400fa7113bea4f22e40e336e244d91aade8b6fb2c
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_TESTING=OFF
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(
    PACKAGE_NAME EKore
    CONFIG_PATH share/EKore
)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

