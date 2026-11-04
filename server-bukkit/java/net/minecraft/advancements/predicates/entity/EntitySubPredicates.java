// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates.entity;
import com.mojang.serialization.Codec;
import java.util.LinkedHashMap;
import java.util.Map;
public class EntitySubPredicates {
    private static final Map<String, Codec<? extends EntitySubPredicate>> kByName = new LinkedHashMap<>();
    private static final Map<Codec<? extends EntitySubPredicate>, String> kByCodec = new java.util.IdentityHashMap<>();
    private static <T extends EntitySubPredicate> void register(String name, Codec<T> codec) {
        kByName.put(name, codec);
        kByCodec.put(codec, name);
    }
    static {
        register("entity_type", EntityTypePredicate.CODEC);
        register("location", EntityLocationPredicate.CODEC);
        register("stepping_on", SteppingOnPredicate.CODEC);
        register("movement_affected_by", MovementAffectedByPredicate.CODEC);
        register("distance", DistanceToPlayerPredicate.CODEC);
        register("movement", MovementPredicate.CODEC);
        register("effects", EntityEffectsPredicate.CODEC);
        register("nbt", EntityNbtPredicate.CODEC);
        register("flags", EntityFlagsPredicate.CODEC);
        register("equipment", EntityEquipmentPredicate.CODEC);
        register("periodic_tick", PeriodicEntityTickPredicate.CODEC);
        register("vehicle", VehiclePredicate.CODEC);
        register("passenger", PassengerPredicate.CODEC);
        register("targeted_entity", TargetedEntityPredicate.CODEC);
        register("team", TeamPredicate.CODEC);
        register("slots", EntitySlotsPredicate.CODEC);
        register("components", EntityExactDataComponentsPredicate.CODEC);
        register("predicates", EntityPartialComponentsPredicate.CODEC);
        register("entity_tags", EntityTagPredicate.CODEC);
        register("type_specific/lightning", LightningBoltPredicate.CODEC);
        register("type_specific/fishing_hook", FishingHookPredicate.CODEC);
        register("type_specific/player", PlayerPredicate.CODEC);
        register("type_specific/cube_mob", CubeMobPredicate.CODEC);
        register("type_specific/raider", RaiderPredicate.CODEC);
        register("type_specific/sheep", SheepPredicate.CODEC);
    }
    public static Codec<? extends EntitySubPredicate> byName(String name) {
        return kByName.get(name);
    }
    public static String nameOf(Codec<? extends EntitySubPredicate> codec) {
        return kByCodec.get(codec);
    }
}
