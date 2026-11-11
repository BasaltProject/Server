// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.worldupdate;

import java.util.EnumMap;
import java.util.Map;
import net.minecraft.network.chat.Component;
import net.minecraft.util.datafix.DataFixTypes;

public class UpgradeStatusTranslator {
    private static final Component kFailed = Component.translatable("optimizeWorld.stage.failed");
    private static final Component kCounting = Component.translatable("optimizeWorld.stage.counting");
    private static final Component kUpgrading = Component.translatable("optimizeWorld.stage.upgrading");
    private final Map<DataFixTypes, Messages> messages = new EnumMap<>(DataFixTypes.class);

    public UpgradeStatusTranslator() {
        messages.put(DataFixTypes.CHUNK, Messages.create("chunks"));
        messages.put(DataFixTypes.ENTITY_CHUNK, Messages.create("entities"));
        messages.put(DataFixTypes.POI_CHUNK, Messages.create("poi"));
    }

    public Component translate(UpgradeProgress upgradeProgress) {
        UpgradeProgress.Status status = upgradeProgress.getStatus();
        if (status == UpgradeProgress.Status.FAILED) {
            return kFailed;
        }
        DataFixTypes type = upgradeProgress.getDataFixType();
        if (status == UpgradeProgress.Status.COUNTING || type == null) {
            return kCounting;
        }
        Messages forType = messages.get(type);
        return forType == null ? kUpgrading : forType.forStatus(status);
    }

    public static record Messages(Component upgrading, Component finished) {
        public static Messages create(String type) {
            return new Messages(Component.translatable("optimizeWorld.stage.upgrading." + type), Component.translatable("optimizeWorld.stage.finished." + type));
        }

        public Component forStatus(UpgradeProgress.Status status) {
            return switch (status) {
                case UPGRADING -> upgrading;
                case FINISHED -> finished;
                default -> throw new IllegalStateException("Invalid Status received: " + status);
            };
        }
    }
}
