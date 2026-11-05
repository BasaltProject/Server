// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.attribute.modifier;

import com.mojang.serialization.Codec;
import net.minecraft.world.attribute.EnvironmentAttribute;
import net.minecraft.world.attribute.LerpFunction;

public enum BooleanModifier implements AttributeModifier<Boolean, Boolean> {
    AND, NAND, OR, NOR, XOR, XNOR;

    @Override
    public Boolean apply(Boolean subject, Boolean argument) {
        return switch (this) {
            case AND -> subject && argument;
            case NAND -> !(subject && argument);
            case OR -> subject || argument;
            case NOR -> !(subject || argument);
            case XOR -> subject ^ argument;
            case XNOR -> subject == argument;
        };
    }

    @Override
    public Codec<Boolean> argumentCodec(EnvironmentAttribute<Boolean> type) {
        return Codec.BOOL;
    }

    @Override
    public LerpFunction<Boolean> argumentKeyframeLerp(EnvironmentAttribute<Boolean> type) {
        return LerpFunction.ofStep(1.0F);
    }
}
