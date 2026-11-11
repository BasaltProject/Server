// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.attribute.modifier;

import com.mojang.serialization.Codec;
import java.util.List;
import java.util.Map;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.attribute.EnvironmentAttribute;
import net.minecraft.world.attribute.LerpFunction;
import net.minecraft.world.level.biome.MobSpawnSettings;
import org.joml.Vector3fc;
import org.joml.Vector4fc;

public interface AttributeModifier<Subject, Argument> {
    Map<OperationId, AttributeModifier<Boolean, ?>> BOOLEAN_LIBRARY = Map.ofEntries(
        Map.entry(OperationId.OVERRIDE, override()), Map.entry(OperationId.AND, BooleanModifier.AND),
        Map.entry(OperationId.NAND, BooleanModifier.NAND), Map.entry(OperationId.OR, BooleanModifier.OR),
        Map.entry(OperationId.NOR, BooleanModifier.NOR), Map.entry(OperationId.XOR, BooleanModifier.XOR),
        Map.entry(OperationId.XNOR, BooleanModifier.XNOR));
    Map<OperationId, AttributeModifier<Float, ?>> FLOAT_LIBRARY = Map.ofEntries(
        Map.entry(OperationId.OVERRIDE, override()), Map.entry(OperationId.ALPHA_BLEND, FloatModifier.ALPHA_BLEND),
        Map.entry(OperationId.ADD, FloatModifier.ADD), Map.entry(OperationId.SUBTRACT, FloatModifier.SUBTRACT),
        Map.entry(OperationId.MULTIPLY, FloatModifier.MULTIPLY), Map.entry(OperationId.MINIMUM, FloatModifier.MINIMUM),
        Map.entry(OperationId.MAXIMUM, FloatModifier.MAXIMUM));
    Map<OperationId, AttributeModifier<Vector3fc, ?>> RGB_COLOR_LIBRARY = Map.of(OperationId.OVERRIDE, override(),
        OperationId.ALPHA_BLEND, ColorModifier.ALPHA_BLEND_RGB, OperationId.ADD, ColorModifier.ADD_RGB,
        OperationId.SUBTRACT, ColorModifier.SUBTRACT_RGB, OperationId.MULTIPLY, ColorModifier.MULTIPLY_RGB,
        OperationId.BLEND_TO_GRAY, ColorModifier.BLEND_TO_GRAY_RGB);
    Map<OperationId, AttributeModifier<Vector4fc, ?>> ARGB_COLOR_LIBRARY = Map.of(OperationId.OVERRIDE, override(),
        OperationId.ALPHA_BLEND, ColorModifier.ALPHA_BLEND_ARGB, OperationId.ADD, ColorModifier.ADD_ARGB,
        OperationId.SUBTRACT, ColorModifier.SUBTRACT_ARGB, OperationId.MULTIPLY, ColorModifier.MULTIPLY_ARGB,
        OperationId.BLEND_TO_GRAY, ColorModifier.BLEND_TO_GRAY_ARGB);
    Map<OperationId, AttributeModifier<Integer, ?>> INTEGER_LIBRARY = Map.of(OperationId.OVERRIDE, override(),
        OperationId.ADD, IntegerModifier.ADD, OperationId.SUBTRACT, IntegerModifier.SUBTRACT,
        OperationId.MULTIPLY, IntegerModifier.MULTIPLY, OperationId.MINIMUM, IntegerModifier.MINIMUM,
        OperationId.MAXIMUM, IntegerModifier.MAXIMUM);
    Map<OperationId, AttributeModifier<MobSpawnSettings, ?>> MOB_SPAWN_SETTINGS_LIBRARY = Map.of(OperationId.OVERRIDE, override(),
        OperationId.OVERLAY, MobSpawnSettingsModifier.overlay());

    static <Value> AttributeModifier<Value, Value> override() {
        return new OverrideModifier<>();
    }

    static <Element> Map<OperationId, AttributeModifier<List<Element>, ?>> listLibrary() {
        return Map.of(OperationId.OVERRIDE, override(), OperationId.APPEND, ListModifier.append());
    }

    Subject apply(Subject subject, Argument argument);

    Codec<Argument> argumentCodec(EnvironmentAttribute<Subject> attribute);

    LerpFunction<Argument> argumentKeyframeLerp(EnvironmentAttribute<Subject> attribute);

    record OverrideModifier<Value>() implements AttributeModifier<Value, Value> {
        @Override
        public Value apply(Value subject, Value argument) {
            return argument;
        }

        @Override
        public Codec<Value> argumentCodec(EnvironmentAttribute<Value> attribute) {
            return attribute.valueCodec();
        }

        @Override
        public LerpFunction<Value> argumentKeyframeLerp(EnvironmentAttribute<Value> attribute) {
            return attribute.type().keyframeLerp();
        }
    }

    enum OperationId implements StringRepresentable {
        OVERRIDE, ALPHA_BLEND, ADD, SUBTRACT, MULTIPLY, BLEND_TO_GRAY, MINIMUM, MAXIMUM, AND, NAND, OR, NOR, XOR, XNOR, APPEND, OVERLAY;
        public static final Codec<OperationId> CODEC = StringRepresentable.fromEnum(OperationId::values);

        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
    }
}
