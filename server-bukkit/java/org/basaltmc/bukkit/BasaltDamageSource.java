package org.basaltmc.bukkit;

import java.util.function.Consumer;
import net.kyori.adventure.pointer.Pointers;
import org.bukkit.Location;
import org.bukkit.damage.DamageSource;
import org.bukkit.damage.DamageType;
import org.bukkit.entity.Entity;

/** A damage source as the API describes it: a type plus whoever caused it and where. */
final class BasaltDamageSource implements DamageSource {
    private final DamageType type;
    private final Entity causing;
    private final Entity direct;
    private final Location location;
    private final Pointers context;

    private BasaltDamageSource(DamageType type, Entity causing, Entity direct, Location location, Pointers context) {
        this.type = type;
        this.causing = causing;
        this.direct = direct;
        this.location = location;
        this.context = context;
    }

    static final class Builder implements DamageSource.Builder {
        private final DamageType type;
        private Entity causing;
        private Entity direct;
        private Location location;
        private final Pointers.Builder pointers = Pointers.builder();

        Builder(DamageType type) {
            this.type = type;
        }

        @Override
        public DamageSource.Builder withCausingEntity(Entity entity) {
            causing = entity;
            return this;
        }

        @Override
        public DamageSource.Builder withDirectEntity(Entity entity) {
            direct = entity;
            return this;
        }

        @Override
        public DamageSource.Builder withDamageLocation(Location at) {
            location = at;
            return this;
        }

        @Override
        public DamageSource.Builder withDamageContext(Consumer<Pointers.Builder> edit) {
            edit.accept(pointers);
            return this;
        }

        @Override
        public DamageSource build() {
            return new BasaltDamageSource(type, causing, direct, location, pointers.build());
        }
    }

    @Override
    public DamageType getDamageType() {
        return type;
    }

    @Override
    public Entity getCausingEntity() {
        return causing;
    }

    @Override
    public Entity getDirectEntity() {
        return direct;
    }

    @Override
    public Location getDamageLocation() {
        return location == null ? null : location.clone();
    }

    @Override
    public Location getSourceLocation() {
        if (location != null) {
            return location.clone();
        }
        Entity source = direct != null ? direct : causing;
        return source == null ? null : source.getLocation();
    }

    @Override
    public boolean isIndirect() {
        return causing != direct;
    }

    @Override
    public float getFoodExhaustion() {
        return type.getExhaustion();
    }

    @Override
    public boolean scalesWithDifficulty() {
        return type.getDamageScaling() == org.bukkit.damage.DamageScaling.ALWAYS
            || (type.getDamageScaling() == org.bukkit.damage.DamageScaling.WHEN_CAUSED_BY_LIVING_NON_PLAYER
                && causing instanceof org.bukkit.entity.LivingEntity && !(causing instanceof org.bukkit.entity.Player));
    }

    @Override
    public Pointers getDamageContext() {
        return context;
    }
}
