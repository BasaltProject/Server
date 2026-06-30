package org.basaltmc.bukkit;

import io.papermc.paper.connection.PlayerCommonConnection;
import io.papermc.paper.dialog.DialogResponseView;
import io.papermc.paper.event.player.PlayerCustomClickEvent;
import net.kyori.adventure.key.Key;
import net.kyori.adventure.nbt.api.BinaryTagHolder;
import org.basaltmc.bukkit.dialog.DialogCallbacks;
import org.bukkit.entity.Player;

/** Gives the answers that come back from a dialog to the function behind the button, or else to the plugins. */
final class DialogEvents {
    private DialogEvents() {}

    private static final class Click extends PlayerCustomClickEvent {
        private final DialogCallbacks.Response response;

        Click(Key id, PlayerCommonConnection connection, String payload) {
            super(id, connection);
            this.response = new DialogCallbacks.Response(payload);
        }

        @Override
        public BinaryTagHolder getTag() {
            return response.payload();
        }

        @Override
        public DialogResponseView getDialogResponseView() {
            return response;
        }
    }

    static void install() {
        EventBridge.map("player.custom_click", Click.class, f -> {
            Player player = f.player();
            String id = f.text("id");
            String payload = f.text("payload");
            if (player == null || DialogCallbacks.run(id, player, payload)) {
                return null;
            }
            return new Click(Key.key(id), new org.basaltmc.bukkit.player.BasaltConnection(player), payload);
        });
    }
}
