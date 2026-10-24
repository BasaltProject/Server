package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.AbstractSkeleton;

public interface AbstractSkeletonOps extends AbstractSkeleton, Cored {
    default boolean shouldBurnInDay() {
        return core().flag("burns_in_day");
    }

    default void setShouldBurnInDay(boolean burn) {
        core().set("burns_in_day", burn);
    }

    default org.bukkit.entity.Skeleton.SkeletonType getSkeletonType() {
        return switch (core().type.getKey().getKey()) {
            case "wither_skeleton" -> org.bukkit.entity.Skeleton.SkeletonType.WITHER;
            case "stray" -> org.bukkit.entity.Skeleton.SkeletonType.STRAY;
            case "bogged" -> org.bukkit.entity.Skeleton.SkeletonType.BOGGED;
            case "parched" -> org.bukkit.entity.Skeleton.SkeletonType.PARCHED;
            default -> org.bukkit.entity.Skeleton.SkeletonType.NORMAL;
        };
    }

    default void setSkeletonType(org.bukkit.entity.Skeleton.SkeletonType type) {
        core().retype(switch (type) {
            case WITHER -> "wither_skeleton";
            case STRAY -> "stray";
            case BOGGED -> "bogged";
            case PARCHED -> "parched";
            default -> "skeleton";
        });
    }
}
