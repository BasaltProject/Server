package net.minecraft.network.protocol.game;
public class ClientboundBundlePacket extends net.minecraft.network.protocol.BundlePacket<ClientGamePacketListener> {
    public ClientboundBundlePacket(
        Iterable<net.minecraft.network.protocol.Packet<? super ClientGamePacketListener>> packets) {
        super(packets);
    }
}
