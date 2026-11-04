// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.permissions;

public interface PermissionSet {
    PermissionSet NO_PERMISSIONS = permission -> false;
    PermissionSet ALL_PERMISSIONS = permission -> true;

    boolean hasPermission(Permission permission);

    default PermissionSet union(PermissionSet other) {
        return new PermissionSetUnion(this, other);
    }
}
