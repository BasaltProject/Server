// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.dialog;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.List;
import java.util.Optional;
import net.minecraft.server.dialog.action.Action;
public record ConfirmationDialog(CommonDialogData common, ActionButton yesButton, ActionButton noButton) implements SimpleDialog {
    public static final MapCodec<ConfirmationDialog> MAP_CODEC = RecordCodecBuilder.mapCodec(i -> i.group(
            CommonDialogData.MAP_CODEC.forGetter(ConfirmationDialog::common),
            ActionButton.CODEC.fieldOf("yes").forGetter(ConfirmationDialog::yesButton),
            ActionButton.CODEC.fieldOf("no").forGetter(ConfirmationDialog::noButton))
            .apply(i, ConfirmationDialog::new));
    public MapCodec<ConfirmationDialog> codec() {
        return MAP_CODEC;
    }
    public Optional<Action> onCancel() {
        return noButton.action();
    }
    public List<ActionButton> mainActions() {
        return List.of(yesButton, noButton);
    }
}
