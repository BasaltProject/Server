// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat;
import java.util.UUID;
import net.minecraft.util.Signer;
import net.minecraft.world.entity.player.ProfileKeyPair;
public record LocalChatSession(UUID sessionId, ProfileKeyPair keyPair) {
    public static LocalChatSession create(ProfileKeyPair keyPair) {
        return new LocalChatSession(UUID.randomUUID(), keyPair);
    }
    public SignedMessageChain.Encoder createMessageEncoder(UUID profileId) {
        return new SignedMessageChain(profileId, sessionId).encoder(Signer.from(keyPair.privateKey(), "SHA256withRSA"));
    }
    public RemoteChatSession asRemote() {
        return new RemoteChatSession(sessionId, keyPair.publicKey());
    }
}
