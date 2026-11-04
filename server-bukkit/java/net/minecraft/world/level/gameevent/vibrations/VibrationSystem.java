// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.gameevent.vibrations;

import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.function.ToIntFunction;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.tags.BlockTags;
import net.minecraft.tags.GameEventTags;
import net.minecraft.tags.TagKey;
import net.minecraft.util.ExtraCodecs;
import net.minecraft.util.Mth;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.level.gameevent.GameEventListener;
import net.minecraft.world.level.gameevent.PositionSource;
import net.minecraft.world.phys.Vec3;

public interface VibrationSystem {
    List<ResourceKey<GameEvent>> RESONANCE_EVENTS = resonanceKeys();
    int NO_VIBRATION_FREQUENCY = 0;
    ToIntFunction<ResourceKey<GameEvent>> VIBRATION_FREQUENCY_FOR_EVENT = key -> Frequencies.TABLE.getOrDefault(key.identifier().getPath(), 0);

    Data getVibrationData();

    User getVibrationUser();

    static int getGameEventFrequency(Holder<GameEvent> event) {
        return event.unwrapKey().map(VibrationSystem::getGameEventFrequency).orElse(0);
    }

    static int getGameEventFrequency(ResourceKey<GameEvent> event) {
        return VIBRATION_FREQUENCY_FOR_EVENT.applyAsInt(event);
    }

    static ResourceKey<GameEvent> getResonanceEventByFrequency(int vibrationFrequency) {
        return RESONANCE_EVENTS.get(vibrationFrequency - 1);
    }

    static int getRedstoneStrengthForDistance(float distance, int listenerRadius) {
        return Math.max(1, 15 - Mth.floor(15.0 / listenerRadius * distance));
    }

    private static List<ResourceKey<GameEvent>> resonanceKeys() {
        List<ResourceKey<GameEvent>> keys = new ArrayList<>();
        for (int i = 1; i <= 15; i++) {
            keys.add(ResourceKey.create(Registries.GAME_EVENT, Identifier.withDefaultNamespace("resonate_" + i)));
        }
        return List.copyOf(keys);
    }

    final class Frequencies {
        static final Map<String, Integer> TABLE = build();

        private Frequencies() {
        }

        private static Map<String, Integer> build() {
            Map<String, Integer> table = new java.util.HashMap<>();
            String[][] groups = {
                {"step", "swim", "flap"}, {"projectile_land", "hit_ground", "splash", "bounce"},
                {"item_interact_finish", "projectile_shoot", "instrument_play"}, {"entity_action", "elytra_glide", "unequip"},
                {"entity_dismount", "equip"}, {"entity_interact", "shear", "entity_mount"}, {"entity_damage"}, {"drink", "eat"},
                {"container_close", "block_close", "block_deactivate", "block_detach"},
                {"container_open", "block_open", "block_activate", "block_attach", "prime_fuse", "note_block_play"},
                {"block_change"}, {"block_destroy", "fluid_pickup"}, {"block_place", "fluid_place"},
                {"entity_place", "lightning_strike", "teleport"}, {"entity_die", "explode"}};
            for (int level = 0; level < groups.length; level++) {
                for (String name : groups[level]) {
                    table.put(name, level + 1);
                }
            }
            for (int i = 1; i <= 15; i++) {
                table.put("resonate_" + i, i);
            }
            return table;
        }
    }

    public static final class Data {
        public static final Codec<Data> CODEC = RecordCodecBuilder.create(instance -> instance.group(
            VibrationInfo.CODEC.lenientOptionalFieldOf("event").forGetter(data -> Optional.ofNullable(data.currentVibration)),
            VibrationSelector.CODEC.fieldOf("selector").forGetter(Data::getSelectionStrategy),
            ExtraCodecs.optionalAlwaysPresentFieldOf(ExtraCodecs.NON_NEGATIVE_INT, "event_delay", 0).forGetter(Data::getTravelTimeInTicks))
            .apply(instance, (event, selector, delay) -> new Data(event.orElse(null), selector, delay, true)));
        public static final String NBT_TAG_KEY = "listener";
        private VibrationInfo currentVibration;
        private int travelTimeInTicks;
        private final VibrationSelector selectionStrategy;
        private boolean reloadVibrationParticle;

        private Data(VibrationInfo currentVibration, VibrationSelector selectionStrategy, int travelTimeInTicks, boolean reloadVibrationParticle) {
            this.currentVibration = currentVibration;
            this.travelTimeInTicks = travelTimeInTicks;
            this.selectionStrategy = selectionStrategy;
            this.reloadVibrationParticle = reloadVibrationParticle;
        }

        public Data() {
            this(null, new VibrationSelector(), 0, false);
        }

        public VibrationSelector getSelectionStrategy() {
            return selectionStrategy;
        }

        public VibrationInfo getCurrentVibration() {
            return currentVibration;
        }

        public void setCurrentVibration(VibrationInfo currentVibration) {
            this.currentVibration = currentVibration;
        }

        public int getTravelTimeInTicks() {
            return travelTimeInTicks;
        }

        public void setTravelTimeInTicks(int travelTimeInTicks) {
            this.travelTimeInTicks = travelTimeInTicks;
        }

        public void decrementTravelTime() {
            travelTimeInTicks = Math.max(0, travelTimeInTicks - 1);
        }

        public boolean shouldReloadVibrationParticle() {
            return reloadVibrationParticle;
        }

        public void setReloadVibrationParticle(boolean reloadVibrationParticle) {
            this.reloadVibrationParticle = reloadVibrationParticle;
        }
    }

    public static class Listener implements GameEventListener {
        private final VibrationSystem system;

        public Listener(VibrationSystem system) {
            this.system = system;
        }

        public PositionSource getListenerSource() {
            return system.getVibrationUser().getPositionSource();
        }

        public int getListenerRadius() {
            return system.getVibrationUser().getListenerRadius();
        }

        public boolean handleGameEvent(ServerLevel level, Holder<GameEvent> event, GameEvent.Context context, Vec3 sourcePosition) {
            Data data = system.getVibrationData();
            User user = system.getVibrationUser();
            if (data.getCurrentVibration() != null || !user.isValidVibration(event, context)) {
                return false;
            }
            Optional<Vec3> target = user.getPositionSource().getPosition(level);
            if (target.isEmpty() || !user.canReceiveVibration(level, BlockPos.containing(sourcePosition), event, context)
                || isOccluded(level, sourcePosition, target.get())) {
                return false;
            }
            schedule(level, data, event, context, sourcePosition, target.get());
            return true;
        }

        public void forceScheduleVibration(ServerLevel level, Holder<GameEvent> event, GameEvent.Context context, Vec3 origin) {
            system.getVibrationUser().getPositionSource().getPosition(level)
                .ifPresent(target -> schedule(level, system.getVibrationData(), event, context, origin, target));
        }

        private void schedule(ServerLevel level, Data data, Holder<GameEvent> event, GameEvent.Context context, Vec3 origin, Vec3 target) {
            data.getSelectionStrategy().addCandidate(new VibrationInfo(event, (float) origin.distanceTo(target), origin, context.sourceEntity()), level.getGameTime());
        }

        public static float distanceBetweenInBlocks(BlockPos origin, BlockPos dest) {
            return (float) Math.sqrt(origin.distSqr(dest));
        }

        private static boolean isOccluded(Level level, Vec3 origin, Vec3 dest) {
            Vec3 from = new Vec3(Mth.floor(origin.x) + 0.5, Mth.floor(origin.y) + 0.5, Mth.floor(origin.z) + 0.5);
            Vec3 to = new Vec3(Mth.floor(dest.x) + 0.5, Mth.floor(dest.y) + 0.5, Mth.floor(dest.z) + 0.5);
            int steps = Math.max(1, (int) Math.ceil(from.distanceTo(to) * 4));
            for (int i = 1; i < steps; i++) {
                double t = (double) i / steps;
                BlockPos at = BlockPos.containing(Mth.lerp(t, from.x, to.x), Mth.lerp(t, from.y, to.y), Mth.lerp(t, from.z, to.z));
                if (!at.equals(BlockPos.containing(from)) && !at.equals(BlockPos.containing(to))
                    && level.getBlockState(at).is(BlockTags.OCCLUDES_VIBRATION_SIGNALS)) {
                    return true;
                }
            }
            return false;
        }
    }

    public interface Ticker {
        static void tick(Level level, Data data, User user) {
            if (!(level instanceof ServerLevel serverLevel)) {
                return;
            }
            if (data.getCurrentVibration() == null) {
                data.getSelectionStrategy().chosenCandidate(serverLevel.getGameTime()).ifPresent(chosen -> {
                    data.setCurrentVibration(chosen);
                    data.setTravelTimeInTicks(user.calculateTravelTimeInTicks(chosen.distance()));
                    user.onDataChanged();
                    data.getSelectionStrategy().startOver();
                });
            }
            VibrationInfo current = data.getCurrentVibration();
            if (current == null) {
                return;
            }
            boolean changed = data.getTravelTimeInTicks() > 0;
            data.decrementTravelTime();
            if (data.getTravelTimeInTicks() <= 0) {
                changed = deliver(serverLevel, data, user, current);
            }
            if (changed) {
                user.onDataChanged();
            }
        }

        private static boolean deliver(ServerLevel level, Data data, User user, VibrationInfo vibration) {
            BlockPos origin = BlockPos.containing(vibration.pos());
            BlockPos destination = user.getPositionSource().getPosition(level).map(BlockPos::containing).orElse(origin);
            user.onReceiveVibration(level, origin, vibration.gameEvent(), vibration.getEntity(level).orElse(null),
                vibration.getProjectileOwner(level).orElse(null), Listener.distanceBetweenInBlocks(origin, destination));
            data.setCurrentVibration(null);
            return true;
        }
    }

    public interface User {
        int getListenerRadius();

        PositionSource getPositionSource();

        boolean canReceiveVibration(ServerLevel level, BlockPos pos, Holder<GameEvent> event, GameEvent.Context context);

        void onReceiveVibration(ServerLevel level, BlockPos pos, Holder<GameEvent> event, Entity sourceEntity, Entity projectileOwner, float receivingDistance);

        default TagKey<GameEvent> getListenableEvents() {
            return GameEventTags.VIBRATIONS;
        }

        default boolean canTriggerAvoidVibration() {
            return false;
        }

        default boolean requiresAdjacentChunksToBeTicking() {
            return false;
        }

        default int calculateTravelTimeInTicks(float distanceToDestination) {
            return Mth.floor(distanceToDestination);
        }

        default boolean isValidVibration(Holder<GameEvent> event, GameEvent.Context context) {
            if (!event.is(getListenableEvents())) {
                return false;
            }
            Entity source = context.sourceEntity();
            if (source != null && (source.isSpectator() || source.dampensVibrations()
                || source.isSteppingCarefully() && event.is(GameEventTags.IGNORE_VIBRATIONS_SNEAKING))) {
                return false;
            }
            return context.affectedState() == null || !context.affectedState().is(BlockTags.DAMPENS_VIBRATIONS);
        }

        default void onDataChanged() {
        }
    }
}
