// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.permissions;

import it.unimi.dsi.fastutil.objects.ReferenceArraySet;
import it.unimi.dsi.fastutil.objects.ReferenceSet;

public class PermissionSetUnion implements PermissionSet {
    private final ReferenceSet<PermissionSet> permissions = new ReferenceArraySet<>();

    PermissionSetUnion(PermissionSet first, PermissionSet second) {
        addAll(first);
        addAll(second);
    }

    private PermissionSetUnion(ReferenceSet<PermissionSet> oldPermissions, PermissionSet other) {
        permissions.addAll(oldPermissions);
        addAll(other);
    }

    private PermissionSetUnion(ReferenceSet<PermissionSet> oldPermissions, ReferenceSet<PermissionSet> other) {
        permissions.addAll(oldPermissions);
        permissions.addAll(other);
    }

    private void addAll(PermissionSet set) {
        if (set instanceof PermissionSetUnion union) {
            permissions.addAll(union.permissions);
        } else {
            permissions.add(set);
        }
    }

    @Override
    public boolean hasPermission(Permission permission) {
        for (PermissionSet set : permissions) {
            if (set.hasPermission(permission)) {
                return true;
            }
        }
        return false;
    }

    @Override
    public PermissionSet union(PermissionSet other) {
        return new PermissionSetUnion(permissions, other);
    }

    public ReferenceSet<PermissionSet> getPermissions() {
        return permissions;
    }
}
