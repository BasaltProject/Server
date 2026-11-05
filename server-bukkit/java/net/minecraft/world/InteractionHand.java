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
        return this == MAIN_HAND ? mainArm : mainArm.getOpposite();
    }
    public EquipmentSlot asEquipmentSlot() {
        return this == MAIN_HAND ? EquipmentSlot.MAINHAND : EquipmentSlot.OFFHAND;
    }
}
