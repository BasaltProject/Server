// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.schedule;

import net.minecraft.core.MappedRegistry;
import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;

public class Activity {
    @SuppressWarnings({"unchecked", "rawtypes"})
    private static final MappedRegistry<Activity> REGISTRY = new MappedRegistry<>(
        (ResourceKey) ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("activity")));

    public static final Activity CORE = register("core");
    public static final Activity IDLE = register("idle");
    public static final Activity WORK = register("work");
    public static final Activity PLAY = register("play");
    public static final Activity REST = register("rest");
    public static final Activity MEET = register("meet");
    public static final Activity PANIC = register("panic");
    public static final Activity RAID = register("raid");
    public static final Activity PRE_RAID = register("pre_raid");
    public static final Activity HIDE = register("hide");
    public static final Activity FIGHT = register("fight");
    public static final Activity CELEBRATE = register("celebrate");
    public static final Activity ADMIRE_ITEM = register("admire_item");
    public static final Activity AVOID = register("avoid");
    public static final Activity RIDE = register("ride");
    public static final Activity PLAY_DEAD = register("play_dead");
    public static final Activity LONG_JUMP = register("long_jump");
    public static final Activity RAM = register("ram");
    public static final Activity TONGUE = register("tongue");
    public static final Activity SWIM = register("swim");
    public static final Activity LAY_SPAWN = register("lay_spawn");
    public static final Activity SNIFF = register("sniff");
    public static final Activity INVESTIGATE = register("investigate");
    public static final Activity ROAR = register("roar");
    public static final Activity EMERGE = register("emerge");
    public static final Activity DIG = register("dig");

    private final String name;

    private Activity(String name) {
        this.name = name;
    }

    public static MappedRegistry<Activity> registry() {
        return REGISTRY;
    }

    public String getName() {
        return name;
    }

    private static Activity register(String name) {
        Activity made = new Activity(name);
        REGISTRY.register(Identifier.withDefaultNamespace(name), made);
        return made;
    }

    @Override
    public String toString() {
        return name;
    }
}
