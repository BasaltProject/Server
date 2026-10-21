package net.minecraft.network;

import io.netty.buffer.ByteBuf;
import net.minecraft.core.RegistryAccess;

public class RegistryFriendlyByteBuf extends FriendlyByteBuf {
    private final RegistryAccess access;

    public RegistryFriendlyByteBuf(ByteBuf source, RegistryAccess access) {
        super(source);
        this.access = access;
    }

    public RegistryAccess registryAccess() {
        return access;
    }
}
