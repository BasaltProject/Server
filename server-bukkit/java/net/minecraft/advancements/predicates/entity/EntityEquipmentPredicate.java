// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates.entity;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.*;
import net.minecraft.advancements.predicates.*;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.*;
import net.minecraft.world.phys.Vec3;
public record EntityEquipmentPredicate(Optional<ItemPredicate> head, Optional<ItemPredicate> chest, Optional<ItemPredicate> legs, Optional<ItemPredicate> feet, Optional<ItemPredicate> body, Optional<ItemPredicate> mainhand, Optional<ItemPredicate> offhand) implements EntitySubPredicate {
    public static final Codec<EntityEquipmentPredicate> CODEC = RecordCodecBuilder.create(i -> i.group(
        ItemPredicate.CODEC.optionalFieldOf("head").forGetter(EntityEquipmentPredicate::head),
        ItemPredicate.CODEC.optionalFieldOf("chest").forGetter(EntityEquipmentPredicate::chest),
        ItemPredicate.CODEC.optionalFieldOf("legs").forGetter(EntityEquipmentPredicate::legs),
        ItemPredicate.CODEC.optionalFieldOf("feet").forGetter(EntityEquipmentPredicate::feet),
        ItemPredicate.CODEC.optionalFieldOf("body").forGetter(EntityEquipmentPredicate::body),
        ItemPredicate.CODEC.optionalFieldOf("mainhand").forGetter(EntityEquipmentPredicate::mainhand),
        ItemPredicate.CODEC.optionalFieldOf("offhand").forGetter(EntityEquipmentPredicate::offhand)).apply(i, EntityEquipmentPredicate::new));
    public boolean matches(Entity entity) {
        if (!(entity instanceof LivingEntity living)) {
            return false;
        }
        if (head.isPresent() && !head.get().test(living.getItemBySlot(EquipmentSlot.HEAD))) {
            return false;
        }
        if (chest.isPresent() && !chest.get().test(living.getItemBySlot(EquipmentSlot.CHEST))) {
            return false;
        }
        if (legs.isPresent() && !legs.get().test(living.getItemBySlot(EquipmentSlot.LEGS))) {
            return false;
        }
        if (feet.isPresent() && !feet.get().test(living.getItemBySlot(EquipmentSlot.FEET))) {
            return false;
        }
        if (body.isPresent() && !body.get().test(living.getItemBySlot(EquipmentSlot.BODY))) {
            return false;
        }
        if (mainhand.isPresent() && !mainhand.get().test(living.getItemBySlot(EquipmentSlot.MAINHAND))) {
            return false;
        }
        if (offhand.isPresent() && !offhand.get().test(living.getItemBySlot(EquipmentSlot.OFFHAND))) {
            return false;
        }
        return true;
    }
    @Override
    public boolean matches(Entity entity, ServerLevel level, Vec3 position) {
        return matches(entity);
    }
    public static class Builder {
        private Optional<ItemPredicate> head = Optional.empty();
        private Optional<ItemPredicate> chest = Optional.empty();
        private Optional<ItemPredicate> legs = Optional.empty();
        private Optional<ItemPredicate> feet = Optional.empty();
        private Optional<ItemPredicate> body = Optional.empty();
        private Optional<ItemPredicate> mainhand = Optional.empty();
        private Optional<ItemPredicate> offhand = Optional.empty();
        public static Builder equipment() {
            return new Builder();
        }
        public Builder head(ItemPredicate.Builder predicate) {
            head = Optional.of(predicate.build());
            return this;
        }
        public Builder chest(ItemPredicate.Builder predicate) {
            chest = Optional.of(predicate.build());
            return this;
        }
        public Builder legs(ItemPredicate.Builder predicate) {
            legs = Optional.of(predicate.build());
            return this;
        }
        public Builder feet(ItemPredicate.Builder predicate) {
            feet = Optional.of(predicate.build());
            return this;
        }
        public Builder body(ItemPredicate.Builder predicate) {
            body = Optional.of(predicate.build());
            return this;
        }
        public Builder mainhand(ItemPredicate.Builder predicate) {
            mainhand = Optional.of(predicate.build());
            return this;
        }
        public Builder offhand(ItemPredicate.Builder predicate) {
            offhand = Optional.of(predicate.build());
            return this;
        }
        public EntityEquipmentPredicate build() {
            return new EntityEquipmentPredicate(head, chest, legs, feet, body, mainhand, offhand);
        }
    }
}
