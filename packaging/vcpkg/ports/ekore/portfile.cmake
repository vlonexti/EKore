vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO vlonexti/EKore
    REF "v${VERSION}"
    SHA512 b440f0519d1b4329a3cd0cd7de84c8fce304c6a1ad88541a95189308e9a1fc78390ecea40cdf33b98e80ab5d55ed522e59c4191a82a5eafd22e67bff9dae1fa1
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
