package org.basaltmc.bukkit.misc;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.UUID;
import org.basaltmc.bukkit.MiscNatives;
import org.basaltmc.bukkit.Natives;
import org.bukkit.permissions.PermissibleBase;
import org.bukkit.permissions.PermissionAttachmentInfo;
import org.bukkit.permissions.ServerOperator;

/** Bukkit's permissible over the server's groups: attachments answer first, then the group, then plugin defaults. */
public final class BasaltPermissible extends PermissibleBase {
    private final UUID id;
    private boolean ready;

    public BasaltPermissible(ServerOperator operator, UUID id) {
        super(operator);
        this.id = id;
        this.ready = true;
        publish();
    }

    @Override
    public boolean hasPermission(String name) {
        if (isPermissionSet(name)) {
            return super.hasPermission(name);
        }
        if (id != null && Natives.playerMay(id.toString(), name)) {
            return true;
        }
        return super.hasPermission(name);
    }

    @Override
    public boolean hasPermission(org.bukkit.permissions.Permission permission) {
        return hasPermission(permission.getName());
    }

    @Override
    public void recalculatePermissions() {
        super.recalculatePermissions();
        if (ready) {
            publish();
        }
    }

    /** Hands the attachments' answers to the server; called again when the player has joined. */
    public void publish() {
        if (id == null) {
            return;
        }
        Set<PermissionAttachmentInfo> effective = getEffectivePermissions();
        List<String> nodes = new ArrayList<>(effective.size());
        boolean[] values = new boolean[effective.size()];
        int index = 0;
        for (PermissionAttachmentInfo info : effective) {
            nodes.add(info.getPermission().toLowerCase(Locale.ROOT));
            values[index++] = info.getValue();
        }
        MiscNatives.playerPermissions(id.toString(), nodes.toArray(new String[0]), values);
    }
}
