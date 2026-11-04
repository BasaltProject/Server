// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;

import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.protocol.CodecModifier;
import net.minecraft.network.protocol.SimpleUnboundProtocol;
import net.minecraft.network.protocol.UnboundProtocol;
public class GameProtocols {
    protected GameProtocols() {
    }

    public interface Context {
            default boolean hasInfiniteMaterials() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.GameProtocols.Context#hasInfiniteMaterials");
        }
        default boolean canUseCommandBlocks() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.GameProtocols.Context#canUseCommandBlocks");
        }
}
    public static final CodecModifier<RegistryFriendlyByteBuf, ServerboundSetCreativeModeSlotPacket, Context> HAS_INFINITE_MATERIALS = null;
    public static final UnboundProtocol<ServerGamePacketListener, RegistryFriendlyByteBuf, Context> SERVERBOUND_TEMPLATE = null;
    public static final SimpleUnboundProtocol<ClientGamePacketListener, RegistryFriendlyByteBuf> CLIENTBOUND_TEMPLATE = null;
}
