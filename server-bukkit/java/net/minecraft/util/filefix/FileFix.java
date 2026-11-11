// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix;

import com.mojang.datafixers.DataFixUtils;
import com.mojang.datafixers.schemas.Schema;
import java.io.IOException;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.util.filefix.access.FileAccessProvider;
import net.minecraft.util.filefix.operations.FileFixOperation;
import net.minecraft.util.filefix.operations.ModifyContent;
import net.minecraft.util.worldupdate.UpgradeProgress;

public abstract class FileFix {
    private final Schema schema;
    private final List<FileFixOperation> fileFixOperations = new ArrayList<>();

    public FileFix(Schema schema) {
        this.schema = schema;
        makeFixer();
    }

    public final Schema getSchema() {
        return schema;
    }

    public final int getVersion() {
        return DataFixUtils.getVersion(schema.getVersionKey());
    }

    public void addFileFixOperation(FileFixOperation operation) {
        fileFixOperations.add(operation);
    }

    public void addFileContentFix(ModifyContent.FileAccessFunction accessFunction) {
        FileAccessProvider provider = new FileAccessProvider(getVersion());
        ModifyContent.FixFunction fix = accessFunction.make(provider);
        provider.freeze();
        fileFixOperations.add(new ModifyContent(provider, fix));
    }

    public int countFileOperations() {
        return fileFixOperations.size();
    }

    public void runFixOperations(Path baseFolder, UpgradeProgress progress) throws IOException {
        for (FileFixOperation operation : fileFixOperations) {
            if (progress.isCanceled()) {
                throw new CanceledFileFixException();
            }
            operation.fix(baseFolder, progress);
            progress.incrementFinishedOperations();
        }
    }

    public abstract void makeFixer();
}
