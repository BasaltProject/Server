// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.permissions;

public interface LevelBasedPermissionSet extends PermissionSet {
    LevelBasedPermissionSet ALL = new Fixed(PermissionLevel.ALL);
    LevelBasedPermissionSet MODERATOR = new Fixed(PermissionLevel.MODERATORS);
    LevelBasedPermissionSet GAMEMASTER = new Fixed(PermissionLevel.GAMEMASTERS);
    LevelBasedPermissionSet ADMIN = new Fixed(PermissionLevel.ADMINS);
    LevelBasedPermissionSet OWNER = new Fixed(PermissionLevel.OWNERS);

    PermissionLevel level();

    @Override
    default boolean hasPermission(Permission permission) {
        if (permission instanceof Permission.HasCommandLevel required) {
            return level().isEqualOrHigherThan(required.level());
        }
        if (permission == Permissions.COMMANDS_ENTITY_SELECTORS) {
            return level().isEqualOrHigherThan(PermissionLevel.GAMEMASTERS);
        }
        return Permissions.CHAT_PERMISSIONS.contains(permission);
    }

    @Override
    default PermissionSet union(PermissionSet other) {
        if (other instanceof LevelBasedPermissionSet levelSet) {
            return level().isEqualOrHigherThan(levelSet.level()) ? this : levelSet;
        }
        return PermissionSet.super.union(other);
    }

    static LevelBasedPermissionSet forLevel(PermissionLevel level) {
        return switch (level) {
            case ALL -> ALL;
            case MODERATORS -> MODERATOR;
            case GAMEMASTERS -> GAMEMASTER;
            case ADMINS -> ADMIN;
            case OWNERS -> OWNER;
        };
    }

    final class Fixed implements LevelBasedPermissionSet {
        private final PermissionLevel level;

        private Fixed(PermissionLevel level) {
            this.level = level;
        }

        @Override
        public PermissionLevel level() {
            return level;
        }
    }
}
