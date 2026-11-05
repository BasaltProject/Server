// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.item.trading.MerchantOffers;
public class ClientboundMerchantOffersPacket implements Packet<ClientGamePacketListener> {
    private final int containerId;
    private final MerchantOffers offers;
    private final int villagerLevel;
    private final int villagerXp;
    private final boolean showProgress;
    private final boolean canRestock;
    public static final StreamCodec<RegistryFriendlyByteBuf, ClientboundMerchantOffersPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundMerchantOffersPacket", () -> Packet.codec(ClientboundMerchantOffersPacket::write, ClientboundMerchantOffersPacket::new));
    public ClientboundMerchantOffersPacket(int containerId, MerchantOffers offers, int merchantLevel, int merchantXp, boolean showProgress, boolean canRestock) {
        this.containerId = containerId;
        this.offers = offers.copy();
        this.villagerLevel = merchantLevel;
        this.villagerXp = merchantXp;
        this.showProgress = showProgress;
        this.canRestock = canRestock;
    }
    private ClientboundMerchantOffersPacket(RegistryFriendlyByteBuf input) {
        this.containerId = input.readContainerId();
        this.offers = MerchantOffers.STREAM_CODEC.decode(input);
        this.villagerLevel = input.readVarInt();
        this.villagerXp = input.readVarInt();
        this.showProgress = input.readBoolean();
        this.canRestock = input.readBoolean();
    }
    private void write(RegistryFriendlyByteBuf output) {
        output.writeContainerId(this.containerId);
        MerchantOffers.STREAM_CODEC.encode(output, this.offers);
        output.writeVarInt(this.villagerLevel);
        output.writeVarInt(this.villagerXp);
        output.writeBoolean(this.showProgress);
        output.writeBoolean(this.canRestock);
    }
    public PacketType<ClientboundMerchantOffersPacket> type() {
        return GamePacketTypes.CLIENTBOUND_MERCHANT_OFFERS;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleMerchantOffers(this);
    }
    public int getContainerId() {
        return containerId;
    }
    public MerchantOffers getOffers() {
        return offers;
    }
    public int getVillagerLevel() {
        return villagerLevel;
    }
    public int getVillagerXp() {
        return villagerXp;
    }
    public boolean showProgress() {
        return showProgress;
    }
    public boolean canRestock() {
        return canRestock;
    }
}
