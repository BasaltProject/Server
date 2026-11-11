// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix.operations;

import java.io.IOException;
import java.io.UncheckedIOException;
import java.nio.file.Path;
import net.minecraft.util.filefix.access.FileAccessProvider;
import net.minecraft.util.worldupdate.UpgradeProgress;

public class ModifyContent implements FileFixOperation {
    private final FileAccessProvider fileAccessProvider;
    private final FixFunction fixFunction;

    public ModifyContent(FileAccessProvider fileAccessProvider, FixFunction fixFunction) {
        this.fileAccessProvider = fileAccessProvider;
        this.fixFunction = fixFunction;
    }

    @Override
    public void fix(Path baseDirectory, UpgradeProgress progress) throws IOException {
        try {
            ScopedValue.where(fileAccessProvider.baseDirectory(), baseDirectory).run(() -> {
                try {
                    fixFunction.run(progress);
                } catch (IOException failure) {
                    throw new UncheckedIOException(failure);
                }
            });
        } catch (UncheckedIOException failure) {
            throw failure.getCause();
        } finally {
            fileAccessProvider.close();
        }
    }

    @FunctionalInterface
    public interface FileAccessFunction {
        FixFunction make(FileAccessProvider fileAccessProvider);
    }

    @FunctionalInterface
    public interface FixFunction {
        void run(UpgradeProgress progress) throws IOException;
    }
}
