"""Handle toolchains missing the unversioned Linux LTO plugin alias."""
from pathlib import Path
import shutil

Import('env')

toolchains = {
    'espressif32': ('toolchain-xtensa-esp32', 'xtensa-esp32-elf'),
}
if env.PioPlatform().name in toolchains:
    package, target = toolchains[env.PioPlatform().name]
    toolchain = Path(env.PioPlatform().get_package_dir(package))
    plugins = sorted(toolchain.glob(f'libexec/gcc/{target}/*/liblto_plugin.so*'))
    if plugins and not (plugins[0].parent / 'liblto_plugin.so').exists():
        # Keep the installed toolchain untouched; GCC searches this build-local
        # directory for the alias, while ar receives the plugin explicitly.
        directory = Path(env.subst('$BUILD_DIR')).resolve() / 'lto-plugin'
        directory.mkdir(parents=True, exist_ok=True)
        plugin = directory / 'liblto_plugin.so'
        shutil.copyfile(plugins[0], plugin)
        prefix = '-B' + directory.as_posix() + '/'
        environments = [env]
        # ESP32 builds its Arduino core with a cloned environment before post
        # scripts run. Apply the alias to that archive environment as well.
        for library in env.Flatten(env.get('LIBS', [])):
            if hasattr(library, 'get_env'):
                library_env = library.get_env()
                if all(library_env is not item for item in environments):
                    environments.append(library_env)
        for build_env in environments:
            build_env.Append(CCFLAGS=[prefix], LINKFLAGS=[prefix])
            build_env.Replace(AR=target + '-ar', RANLIB=target + '-ranlib')
            build_env.Append(ARFLAGS=['--plugin=' + plugin.as_posix()])
            build_env.Append(RANLIBFLAGS=['--plugin=' + plugin.as_posix()])
