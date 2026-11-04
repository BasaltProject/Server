// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.configuration;
import net.minecraft.network.protocol.PacketFlow;
import net.minecraft.network.protocol.PacketType;
public class ConfigurationPacketTypes {
    public static final PacketType<ClientboundCodeOfConductPacket> CLIENTBOUND_CODE_OF_CONDUCT = new PacketType<>(PacketFlow.CLIENTBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("code_of_conduct"));
    public static final PacketType<ClientboundFinishConfigurationPacket> CLIENTBOUND_FINISH_CONFIGURATION = new PacketType<>(PacketFlow.CLIENTBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("finish_configuration"));
    public static final PacketType<ClientboundRegistryDataPacket> CLIENTBOUND_REGISTRY_DATA = new PacketType<>(PacketFlow.CLIENTBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("registry_data"));
    public static final PacketType<ClientboundResetChatPacket> CLIENTBOUND_RESET_CHAT = new PacketType<>(PacketFlow.CLIENTBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("reset_chat"));
    public static final PacketType<ClientboundSelectKnownPacks> CLIENTBOUND_SELECT_KNOWN_PACKS = new PacketType<>(PacketFlow.CLIENTBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("select_known_packs"));
    public static final PacketType<ClientboundUpdateEnabledFeaturesPacket> CLIENTBOUND_UPDATE_ENABLED_FEATURES = new PacketType<>(PacketFlow.CLIENTBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("update_enabled_features"));
    public static final PacketType<ServerboundAcceptCodeOfConductPacket> SERVERBOUND_ACCEPT_CODE_OF_CONDUCT = new PacketType<>(PacketFlow.SERVERBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("accept_code_of_conduct"));
    public static final PacketType<ServerboundFinishConfigurationPacket> SERVERBOUND_FINISH_CONFIGURATION = new PacketType<>(PacketFlow.SERVERBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("finish_configuration"));
    public static final PacketType<ServerboundSelectKnownPacks> SERVERBOUND_SELECT_KNOWN_PACKS = new PacketType<>(PacketFlow.SERVERBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("select_known_packs"));
}
