// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;
import net.minecraft.commands.CommandSource;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.network.chat.Component;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public abstract class BaseCommandBlock {
    private int successCount;
    private boolean trackOutput = true;
    private Component lastOutput;
    private String command = "";
    private Component customName;
    public int getSuccessCount() {
        return successCount;
    }
    public void setSuccessCount(int successCount) {
        this.successCount = successCount;
    }
    public Component getLastOutput() {
        return lastOutput == null ? Component.empty() : lastOutput;
    }
    public void save(ValueOutput output) {
        output.putString("Command", command);
        output.putInt("SuccessCount", successCount);
        output.putBoolean("TrackOutput", trackOutput);
        if (customName != null) {
            output.putString("CustomName", customName.getString());
        }
    }
    public void load(ValueInput input) {
        command = input.getStringOr("Command", "");
        successCount = input.getIntOr("SuccessCount", 0);
        trackOutput = input.getBooleanOr("TrackOutput", true);
        customName = input.getString("CustomName").map(Component::literal).orElse(null);
    }
    public void setCommand(String command) {
        this.command = command;
        this.successCount = 0;
    }
    public String getCommand() {
        return command;
    }
    public boolean performCommand(ServerLevel level) {
        if (command == null || command.isEmpty() || !isValid()) {
            successCount = 0;
            return false;
        }
        boolean ok = org.bukkit.Bukkit.dispatchCommand(org.bukkit.Bukkit.getConsoleSender(), command);
        successCount = ok ? 1 : 0;
        return ok;
    }
    public Component getName() {
        return customName != null ? customName : Component.literal("@");
    }
    public Component getCustomName() {
        return customName;
    }
    public void setCustomName(Component name) {
        this.customName = name;
    }
    public abstract void onUpdated(ServerLevel level);
    public void setLastOutput(Component lastOutput) {
        this.lastOutput = lastOutput;
    }
    public void setTrackOutput(boolean trackOutput) {
        this.trackOutput = trackOutput;
    }
    public boolean isTrackOutput() {
        return trackOutput;
    }
    public abstract CommandSourceStack createCommandSourceStack(ServerLevel level, CommandSource source);
    public abstract boolean isValid();
    protected class CloseableCommandBlockSource implements CommandSource, AutoCloseable {
        protected CloseableCommandBlockSource(ServerLevel level) {
        }
        public boolean acceptsSuccess() {
            return trackOutput;
        }
        public boolean acceptsFailure() {
            return true;
        }
        public boolean shouldInformAdmins() {
            return true;
        }
        public void sendSystemMessage(Component message) {
            if (trackOutput) {
                lastOutput = message;
            }
        }
        public void close() {
        }
    }
}
