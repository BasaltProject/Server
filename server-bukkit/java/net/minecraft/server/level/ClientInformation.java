// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level;

import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.world.entity.HumanoidArm;
import net.minecraft.world.entity.player.ChatVisiblity;

public record ClientInformation(String language, int viewDistance, ChatVisiblity chatVisibility, boolean chatColors,
    int modelCustomisation, HumanoidArm mainHand, boolean textFilteringEnabled, boolean allowsListing, ParticleStatus particleStatus) {
    public static final int MAX_LANGUAGE_LENGTH = 16;

    public ClientInformation(FriendlyByteBuf input) {
        this(input.readUtf(MAX_LANGUAGE_LENGTH), input.readByte(), ChatVisiblity.STREAM_CODEC.decode(input), input.readBoolean(),
            input.readUnsignedByte(), HumanoidArm.STREAM_CODEC.decode(input), input.readBoolean(), input.readBoolean(),
            ParticleStatus.STREAM_CODEC.decode(input));
    }

    public void write(FriendlyByteBuf output) {
        output.writeUtf(language);
        output.writeByte(viewDistance);
        ChatVisiblity.STREAM_CODEC.encode(output, chatVisibility);
        output.writeBoolean(chatColors);
        output.writeByte(modelCustomisation);
        HumanoidArm.STREAM_CODEC.encode(output, mainHand);
        output.writeBoolean(textFilteringEnabled);
        output.writeBoolean(allowsListing);
        ParticleStatus.STREAM_CODEC.encode(output, particleStatus);
    }

    public static ClientInformation createDefault() {
        return new ClientInformation("en_us", 2, ChatVisiblity.FULL, true, 0, HumanoidArm.RIGHT, false, false, ParticleStatus.ALL);
    }
}
