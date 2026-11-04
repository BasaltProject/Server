// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.attributes;

import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import io.netty.buffer.ByteBuf;
import java.util.function.IntFunction;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.Identifier;
import net.minecraft.util.StringRepresentable;

public record AttributeModifier(Identifier id, double amount, Operation operation) {
    private static final Codec<Identifier> ID_CODEC = Codec.STRING.xmap(Identifier::parse, Identifier::toString);

    public static final MapCodec<AttributeModifier> MAP_CODEC = RecordCodecBuilder.mapCodec(instance -> instance.group(
        ID_CODEC.fieldOf("id").forGetter(AttributeModifier::id),
        Codec.DOUBLE.fieldOf("amount").forGetter(AttributeModifier::amount),
        Operation.CODEC.fieldOf("operation").forGetter(AttributeModifier::operation)).apply(instance, AttributeModifier::new));
    public static final Codec<AttributeModifier> CODEC = MAP_CODEC.codec();
    public static final StreamCodec<ByteBuf, AttributeModifier> STREAM_CODEC = StreamCodec.of(
        (buf, modifier) -> {
            ByteBufCodecs.STRING_UTF8.encode(buf, modifier.id.toString());
            ByteBufCodecs.DOUBLE.encode(buf, modifier.amount);
            Operation.STREAM_CODEC.encode(buf, modifier.operation);
        },
        buf -> new AttributeModifier(Identifier.parse(ByteBufCodecs.STRING_UTF8.decode(buf)),
            ByteBufCodecs.DOUBLE.decode(buf), Operation.STREAM_CODEC.decode(buf)));

    public boolean is(Identifier id) {
        return id.equals(this.id);
    }

    public static enum Operation implements StringRepresentable {
        ADD_VALUE("add_value", 0), ADD_MULTIPLIED_BASE("add_multiplied_base", 1), ADD_MULTIPLIED_TOTAL("add_multiplied_total", 2);

        public static final IntFunction<Operation> BY_ID = id -> values()[id];
        public static final StreamCodec<ByteBuf, Operation> STREAM_CODEC = ByteBufCodecs.idMapper(BY_ID, Operation::id);
        public static final Codec<Operation> CODEC = StringRepresentable.fromEnum(Operation::values);

        private final String name;
        private final int id;

        Operation(String name, int id) {
            this.name = name;
            this.id = id;
        }

        public int id() {
            return id;
        }

        public String getSerializedName() {
            return name;
        }
    }
}
