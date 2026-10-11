add_library(reposco_storage STATIC EncryptedDatabase.cpp HistoryStore.cpp)
target_include_directories(reposco_storage PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(
  reposco_storage
  PUBLIC reposco_content
  PRIVATE reposco_sqlcipher)
reposco_owned_target(reposco_storage)
