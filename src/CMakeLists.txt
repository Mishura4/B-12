target_precompile_headers(B12 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/B12.h)

set(COMMAND_SOURCES
    Command.cpp
    Command.h
    CommandTable.h
    MultiCommandProcess.cpp
    MultiCommandProcess.h
    Sticker.cpp
    Study.cpp
    Study.h
)

set(CORE_SOURCES
    Bot.cpp
    Bot.h
    main.cpp
)

set(DATA_SOURCES
    DataStore.cpp
    DataStore.h
    DataStores.cpp
    DataStores.h
    DataStructures.h
    Database.cpp
    Database.h
    DatabaseStatement.h
    DatabaseStatement.inl
    Lang.h
)

set (GUILD_SOURCES
    Guild.cpp
    Guild.h
)

set (USER_SOURCES
    User.cpp
    User.h
)

list(TRANSFORM COMMAND_SOURCES PREPEND ${CMAKE_CURRENT_LIST_DIR}/Commands/)
list(TRANSFORM CORE_SOURCES PREPEND ${CMAKE_CURRENT_LIST_DIR}/Core/)
list(TRANSFORM DATA_SOURCES PREPEND ${CMAKE_CURRENT_LIST_DIR}/Data/)
list(TRANSFORM GUILD_SOURCES PREPEND ${CMAKE_CURRENT_LIST_DIR}/Guild/)
list(TRANSFORM USER_SOURCES PREPEND ${CMAKE_CURRENT_LIST_DIR}/User/)

set(B12_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/B12.cpp
        ${CMAKE_CURRENT_LIST_DIR}/B12.h
        ${COMMAND_SOURCES}
        ${CORE_SOURCES}
        ${DATA_SOURCES}
        ${GUILD_SOURCES}
        ${USER_SOURCES}
    PARENT_SCOPE
)