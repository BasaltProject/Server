// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates.entity;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.DynamicOps;
import com.mojang.serialization.MapLike;
import java.util.Comparator;
import java.util.LinkedHashMap;
import java.util.Map;
import net.minecraft.advancements.predicates.*;
import net.minecraft.core.HolderGetter;
import net.minecraft.core.component.DataComponentExactPredicate;
import net.minecraft.core.component.predicates.DataComponentPredicate;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.tags.TagKey;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.phys.Vec3;
public final class EntityPredicate {
    @SuppressWarnings({"unchecked", "rawtypes"})
    private static final Codec<Map<Codec<? extends EntitySubPredicate>, EntitySubPredicate>> kParts = new Codec<>() {
        @Override
        public <T> DataResult<com.mojang.datafixers.util.Pair<Map<Codec<? extends EntitySubPredicate>, EntitySubPredicate>, T>> decode(DynamicOps<T> ops, T input) {
            return ops.getMap(input).flatMap(map -> {
                Map<Codec<? extends EntitySubPredicate>, EntitySubPredicate> out = new LinkedHashMap<>();
                StringBuilder failure = new StringBuilder();
                map.entries().forEach(entry -> {
                    String name = ops.getStringValue(entry.getFirst()).result().orElse(null);
                    String bare = name != null && name.startsWith("minecraft:") ? name.substring(10) : name;
                    Codec<? extends EntitySubPredicate> codec = bare == null ? null : EntitySubPredicates.byName(bare);
                    if (codec == null) {
                        failure.append("Unknown entity sub predicate: ").append(name);
                        return;
                    }
                    DataResult<? extends EntitySubPredicate> parsed = codec.parse(ops, entry.getSecond());
                    if (parsed.error().isPresent()) {
                        failure.append(parsed.error().get().message());
                    } else {
                        out.put(codec, parsed.result().get());
                    }
                });
                return failure.length() > 0 ? DataResult.<com.mojang.datafixers.util.Pair<Map<Codec<? extends EntitySubPredicate>, EntitySubPredicate>, T>>error(failure::toString)
                    : DataResult.success(com.mojang.datafixers.util.Pair.of(out, input));
            });
        }
        @Override
        public <T> DataResult<T> encode(Map<Codec<? extends EntitySubPredicate>, EntitySubPredicate> parts, DynamicOps<T> ops, T prefix) {
            com.mojang.serialization.RecordBuilder<T> builder = ops.mapBuilder();
            for (Map.Entry<Codec<? extends EntitySubPredicate>, EntitySubPredicate> entry : parts.entrySet()) {
                String name = EntitySubPredicates.nameOf(entry.getKey());
                builder.add(ops.createString(name), ((Codec) entry.getKey()).encodeStart(ops, entry.getValue()));
            }
            return builder.build(prefix);
        }
    };
    public static final Codec<EntityPredicate> CODEC = kParts.xmap(EntityPredicate::new, predicate -> predicate.parts);
    private static final Comparator<Map.Entry<Codec<? extends EntitySubPredicate>, EntitySubPredicate>> kOrder =
        Comparator.comparingInt(entry -> {
            Codec<?> codec = entry.getKey();
            if (codec == EntityTypePredicate.CODEC) {
                return -1;
            }
            if (codec == VehiclePredicate.CODEC || codec == PassengerPredicate.CODEC || codec == TargetedEntityPredicate.CODEC
                || codec == LightningBoltPredicate.CODEC || codec == PlayerPredicate.CODEC) {
                return 1;
            }
            return codec == EntityNbtPredicate.CODEC ? 2 : 0;
        });
    private static final Codec<EntitySubPredicate> kNeverCodec = com.mojang.serialization.MapCodec.<EntitySubPredicate>unit(() -> (entity, level, position) -> false).codec();
    public static final EntityPredicate NEVER = new EntityPredicate(Map.of(kNeverCodec, (entity, level, position) -> false));
    private final Map<Codec<? extends EntitySubPredicate>, EntitySubPredicate> parts;
    private final EntitySubPredicate combined;
    public EntityPredicate(Map<Codec<? extends EntitySubPredicate>, EntitySubPredicate> parts) {
        this.parts = parts;
        this.combined = combine(parts);
    }
    public boolean matches(ServerPlayer player, Entity entity) {
        return matches(player.level(), player.position(), entity);
    }
    public boolean matches(ServerLevel level, Vec3 position, Entity entity) {
        return entity != null && combined.matches(entity, level, position);
    }
    private static EntitySubPredicate combine(Map<Codec<? extends EntitySubPredicate>, EntitySubPredicate> parts) {
        if (parts.isEmpty()) {
            return EntitySubPredicate.ALWAYS_TRUE;
        }
        EntitySubPredicate[] ordered = parts.entrySet().stream().sorted(kOrder).map(Map.Entry::getValue).toArray(EntitySubPredicate[]::new);
        return (entity, level, position) -> {
            for (EntitySubPredicate part : ordered) {
                if (!part.matches(entity, level, position)) {
                    return false;
                }
            }
            return true;
        };
    }
    @Override
    public boolean equals(Object other) {
        return other == this || other instanceof EntityPredicate that && parts.equals(that.parts);
    }
    @Override
    public int hashCode() {
        return parts.hashCode();
    }
    @Override
    public String toString() {
        return "EntityPredicate[parts=" + parts + "]";
    }
    public static class Builder {
        private final Map<Codec<? extends EntitySubPredicate>, EntitySubPredicate> parts = new LinkedHashMap<>();
        public static Builder entity() {
            return new Builder();
        }
        public <T extends EntitySubPredicate> Builder put(Codec<T> key, T predicate) {
            parts.put(key, predicate);
            return this;
        }
        public Builder of(HolderGetter<EntityType<?>> lookup, EntityType<?> entityType) {
            return entityType(EntityTypePredicate.of(lookup, entityType));
        }
        public Builder of(HolderGetter<EntityType<?>> lookup, TagKey<EntityType<?>> tag) {
            return entityType(EntityTypePredicate.of(lookup, tag));
        }
        public Builder entityType(EntityTypePredicate entityType) {
            return put(EntityTypePredicate.CODEC, entityType);
        }
        public Builder distance(DistancePredicate distance) {
            return put(DistanceToPlayerPredicate.CODEC, new DistanceToPlayerPredicate(distance));
        }
        public Builder moving(MovementPredicate movement) {
            return put(MovementPredicate.CODEC, movement);
        }
        public Builder located(LocationPredicate.Builder location) {
            return put(EntityLocationPredicate.CODEC, new EntityLocationPredicate(location.build()));
        }
        public Builder steppingOn(LocationPredicate.Builder location) {
            return put(SteppingOnPredicate.CODEC, new SteppingOnPredicate(location.build()));
        }
        public Builder movementAffectedBy(LocationPredicate.Builder location) {
            return put(MovementAffectedByPredicate.CODEC, new MovementAffectedByPredicate(location.build()));
        }
        public Builder effects(MobEffectsPredicate.Builder effects) {
            return put(EntityEffectsPredicate.CODEC, new EntityEffectsPredicate(effects.build()));
        }
        public Builder nbt(NbtPredicate nbt) {
            return put(EntityNbtPredicate.CODEC, new EntityNbtPredicate(nbt));
        }
        public Builder flags(EntityFlagsPredicate.Builder flags) {
            return put(EntityFlagsPredicate.CODEC, flags.build());
        }
        public Builder equipment(EntityEquipmentPredicate.Builder equipment) {
            return put(EntityEquipmentPredicate.CODEC, equipment.build());
        }
        public Builder equipment(EntityEquipmentPredicate equipment) {
            return put(EntityEquipmentPredicate.CODEC, equipment);
        }
        public Builder periodicTick(int period) {
            return put(PeriodicEntityTickPredicate.CODEC, new PeriodicEntityTickPredicate(period));
        }
        public Builder vehicle(Builder vehicle) {
            return put(VehiclePredicate.CODEC, new VehiclePredicate(vehicle.build()));
        }
        public Builder passenger(Builder passenger) {
            return put(PassengerPredicate.CODEC, new PassengerPredicate(passenger.build()));
        }
        public Builder targetedEntity(Builder targeted) {
            return put(TargetedEntityPredicate.CODEC, new TargetedEntityPredicate(targeted.build()));
        }
        public Builder team(String team) {
            return put(TeamPredicate.CODEC, new TeamPredicate(team));
        }
        public Builder slots(SlotsPredicate slots) {
            return put(EntitySlotsPredicate.CODEC, new EntitySlotsPredicate(slots));
        }
        public Builder components(DataComponentMatchers components) {
            if (!components.exact().isEmpty()) {
                components(components.exact());
            }
            if (!components.partial().isEmpty()) {
                components(components.partial());
            }
            return this;
        }
        public Builder components(DataComponentExactPredicate components) {
            return put(EntityExactDataComponentsPredicate.CODEC, new EntityExactDataComponentsPredicate(components));
        }
        public Builder components(Map<DataComponentPredicate.Type<?>, DataComponentPredicate> components) {
            return put(EntityPartialComponentsPredicate.CODEC, new EntityPartialComponentsPredicate(components));
        }
        public Builder lightingBolt(LightningBoltPredicate lightningBolt) {
            return put(LightningBoltPredicate.CODEC, lightningBolt);
        }
        public Builder player(PlayerPredicate player) {
            return put(PlayerPredicate.CODEC, player);
        }
        public Builder sheep(SheepPredicate sheep) {
            return put(SheepPredicate.CODEC, sheep);
        }
        public Builder cubeMob(CubeMobPredicate cubeMob) {
            return put(CubeMobPredicate.CODEC, cubeMob);
        }
        public Builder raider(RaiderPredicate raider) {
            return put(RaiderPredicate.CODEC, raider);
        }
        public Builder fishingHook(FishingHookPredicate fishingHook) {
            return put(FishingHookPredicate.CODEC, fishingHook);
        }
        public EntityPredicate build() {
            return new EntityPredicate(Map.copyOf(parts));
        }
    }
}
