// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network;

import io.netty.buffer.ByteBuf;
import net.minecraft.core.RegistryAccess;
import java.util.function.Function;

public class RegistryFriendlyByteBuf extends FriendlyByteBuf {
    private final RegistryAccess access;

    public RegistryFriendlyByteBuf(ByteBuf source, RegistryAccess access) {
        super(source);
        this.access = access;
    }

    public RegistryAccess registryAccess() {
        return access;
    }
    public static Function<ByteBuf, RegistryFriendlyByteBuf> decorator(RegistryAccess registryAccess) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.RegistryFriendlyByteBuf#decorator");
    }
}
