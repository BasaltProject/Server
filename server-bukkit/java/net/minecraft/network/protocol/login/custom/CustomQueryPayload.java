// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.login.custom;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.resources.Identifier;
public interface CustomQueryPayload {
    Identifier id();
    void write(FriendlyByteBuf output);
}
