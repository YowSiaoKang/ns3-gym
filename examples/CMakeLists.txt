build_lib_example(
  NAME interference-pattern
  SOURCE_FILES interference-pattern/mygym.cc
               interference-pattern/sim.cc
  LIBRARIES_TO_LINK
    ${libcore}
    ${libinternet}
    ${libopengym}
    ${libwifi}
)

build_lib_example(
  NAME linear-mesh
  SOURCE_FILES linear-mesh/sim.cc
  LIBRARIES_TO_LINK
    ${libapplications}
    ${libcore}
    ${libinternet}
    ${libopengym}
    ${libwifi}
)

build_lib_example(
  NAME linear-mesh-2
  SOURCE_FILES linear-mesh-2/sim.cc
               linear-mesh-2/mygym.cc
  LIBRARIES_TO_LINK
    ${libapplications}
    ${libcore}
    ${libinternet}
    ${libopengym}
    ${libwifi}
)

build_lib_example(
  NAME opengym
  SOURCE_FILES opengym/sim.cc
  LIBRARIES_TO_LINK
    ${libcore}
    ${libopengym}
)

build_lib_example(
  NAME opengym-2
  SOURCE_FILES opengym-2/sim.cc
               opengym-2/mygym.cc
  LIBRARIES_TO_LINK
    ${libcore}
    ${libopengym}
)

build_lib_example(
  NAME rl-tcp
  SOURCE_FILES rl-tcp/sim.cc
               rl-tcp/tcp-rl-env.cc
               rl-tcp/tcp-rl.cc
  LIBRARIES_TO_LINK
    ${libapplications}
    ${libcore}
    ${libflow-monitor}
    ${libinternet}
    ${libopengym}
    ${libpoint-to-point-layout}
    ${libptp}
)

build_lib_example(
  NAME leaf-spine-topology
  SOURCE_FILES DCTCP-baseline/leaf-spine-topology.cc
               DCTCP-baseline/datacenter-workload-generator.cc
               DCTCP-baseline/traffic-analyzer.cc
  LIBRARIES_TO_LINK
    ${libcore}
    ${libnetwork}
    ${libinternet}
    ${libpoint-to-point}
    ${libapplications}
    ${libnetanim}
    ${libmobility}
    ${libtraffic-control}
    ${libinternet-apps}
)
