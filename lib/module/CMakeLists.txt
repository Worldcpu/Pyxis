# lib/module/ — domain modules: flightplan, navdata
# (navdata and router replaced by lib/bravofinder submodule;
#  px_navdata 是 px 自建只读视图——决策 48)
add_subdirectory(flightplan)
add_subdirectory(navdata)
