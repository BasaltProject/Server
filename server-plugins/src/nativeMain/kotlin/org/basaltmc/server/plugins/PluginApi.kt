package org.basaltmc.server.plugins

/**
 * What a plugin sees when it starts: the whole of the API of this stage, put into its globals.
 *
 * Everything registered here goes through the plugin's [PluginRegistrations], so unloading it takes
 * all of it back out. Nothing is a singleton a plugin could reach around the host with.
 */
internal object PluginApi {
    fun install(
        instance: PluginInstance,
        plugin: LoadedPlugin,
        host: PluginHost,
        lanes: Int,
    ) {
        val globals = instance.sandbox.globals
        globals.set("plugin", describe(plugin, instance, lanes))
        globals.set("log", logTable(plugin))
        globals.set("config", tableOf(plugin.config))
        globals.set("files", fileTable(plugin))
        globals.set("wait", Wait(host, plugin, instance))
        globals.set("later", later(host, plugin, instance))
        globals.set("every", every(host, plugin, instance))
        globals.set("cmd", command(host, plugin, instance))
        globals.set("players", playersTable(host, plugin))
        globals.set("worlds", worldsTable(host, plugin))
        globals.set("at", at(host, plugin, instance))
        globals.set("pos", position())
        globals.set("placeholder", placeholder(host, plugin))
        globals.set("fill", fill(host))
        globals.set("broadcast", broadcast(host, plugin))
        globals.set("t", translate(plugin))
        globals.set("service", service(host, plugin))
        globals.set("store", storeTable(host, plugin))
        globals.set("data", dataTable(plugin))
        globals.set("emit", emit(host))
        globals.set("menu", menu(host, plugin))
        // Asked for in the manifest and granted by the operator. Missing either way, the global is
        // not there at all: a plugin cannot test for one and talk its way around it.
        val granted = host.allowed(plugin.manifest.id)
        if (plugin.manifest.capabilities.packets && PluginPermission.PACKETS in granted) {
            globals.set("packet", packets(host, plugin))
        }
        if (plugin.manifest.capabilities.network
                .isNotEmpty() &&
            PluginPermission.NETWORK in granted
        ) {
            globals.set("channel", channel(host, plugin))
        }
        if (plugin.manifest.capabilities.commands && PluginPermission.COMMANDS in granted) {
            globals.set("run", runCommand(host, plugin))
        }
        for (surface in PluginSurface.entries) {
            globals.set(surface.surfaceName, claim(host, plugin, surface))
        }
        globals.set("on", listener(host, plugin, instance, observer = false))
        globals.set("watch", listener(host, plugin, instance, observer = true))
    }
}
