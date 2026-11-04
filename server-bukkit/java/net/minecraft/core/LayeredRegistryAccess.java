// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core;

import java.util.List;
import org.basaltmc.bukkit.facade.ServerAccess;

/** The registries of the server as one set: every layer answers with the registries the native server holds. */
public class LayeredRegistryAccess<T> {
    public LayeredRegistryAccess(List<T> keys) {
    }

    private LayeredRegistryAccess(List<T> keys, List<RegistryAccess.Frozen> values) {
    }

    public RegistryAccess.Frozen getLayer(T layer) {
        return ServerAccess.registries();
    }

    public RegistryAccess.Frozen getAccessForLoading(T forLayer) {
        return ServerAccess.registries();
    }

    public RegistryAccess.Frozen getAccessFrom(T forLayer) {
        return ServerAccess.registries();
    }

    public LayeredRegistryAccess<T> replaceFrom(T fromLayer, RegistryAccess.Frozen... layers) {
        return this;
    }

    public LayeredRegistryAccess<T> replaceFrom(T fromLayer, List<RegistryAccess.Frozen> layers) {
        return this;
    }

    public RegistryAccess.Frozen compositeAccess() {
        return ServerAccess.registries();
    }
}
