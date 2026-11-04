// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;

import net.minecraft.util.StringRepresentable;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
public enum HumanoidArm implements StringRepresentable {
    LEFT, RIGHT;
;
    public static final Codec<HumanoidArm> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.HumanoidArm.class, "CODEC");
    public static final StreamCodec<ByteBuf, HumanoidArm> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.entity.HumanoidArm.class, "STREAM_CODEC");
    public HumanoidArm getOpposite() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.HumanoidArm#getOpposite");
    }
    public Component caption() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.HumanoidArm#caption");
    }
    public String getSerializedName() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.HumanoidArm#getSerializedName");
    }
}
