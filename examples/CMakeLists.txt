add_subdirectory(addressbook)

add_custom_target(examples DEPENDS
  addressbook
)
