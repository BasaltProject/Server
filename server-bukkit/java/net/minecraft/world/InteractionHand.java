// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world;

import io.netty.buffer.ByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.HumanoidArm;
public enum InteractionHand {
    MAIN_HAND, OFF_HAND;
;
    public static final StreamCodec<ByteBuf, InteractionHand> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.InteractionHand.class, "STREAM_CODEC");
    public HumanoidArm asArm(HumanoidArm mainArm) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.InteractionHand#asArm");
    }
    public EquipmentSlot asEquipmentSlot() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.InteractionHand#asEquipmentSlot");
    }
}
