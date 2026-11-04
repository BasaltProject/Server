// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.triggers;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.DynamicOps;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.MapLike;
import com.mojang.serialization.RecordBuilder;
import java.util.stream.Stream;
import net.minecraft.advancements.CriterionTriggerInstance;
import net.minecraft.resources.Identifier;
public record Criterion<T extends CriterionTriggerInstance>(CriterionTrigger<T> trigger, T triggerInstance) {
    private static final MapCodec<Criterion<?>> kMapCodec = new MapCodec<>() {
        @Override
        public <O> Stream<O> keys(DynamicOps<O> ops) {
            return Stream.of(ops.createString("trigger"), ops.createString("conditions"));
        }
        @Override
        public <O> DataResult<Criterion<?>> decode(DynamicOps<O> ops, MapLike<O> input) {
            O type = input.get("trigger");
            if (type == null) {
                return DataResult.error(() -> "Missing trigger");
            }
            return Identifier.CODEC.parse(ops, type).flatMap(id -> {
                CriterionTrigger<?> trigger = CriteriaTriggers.get(id);
                if (trigger == null) {
                    return DataResult.error(() -> "Unknown trigger: " + id);
                }
                O conditions = input.get("conditions");
                return decodeWith(ops, trigger, conditions == null ? ops.emptyMap() : conditions);
            });
        }
        @SuppressWarnings({"unchecked", "rawtypes"})
        private <O> DataResult<Criterion<?>> decodeWith(DynamicOps<O> ops, CriterionTrigger trigger, O conditions) {
            return trigger.codec().parse(ops, conditions).map(instance -> new Criterion(trigger, (CriterionTriggerInstance) instance));
        }
        @SuppressWarnings({"unchecked", "rawtypes"})
        @Override
        public <O> RecordBuilder<O> encode(Criterion<?> criterion, DynamicOps<O> ops, RecordBuilder<O> prefix) {
            Identifier id = CriteriaTriggers.idOf(criterion.trigger());
            prefix.add("trigger", Identifier.CODEC.encodeStart(ops, id));
            prefix.add("conditions", ((Codec) criterion.trigger().codec()).encodeStart(ops, criterion.triggerInstance()));
            return prefix;
        }
    };
    public static final Codec<Criterion<?>> CODEC = kMapCodec.codec();
}
