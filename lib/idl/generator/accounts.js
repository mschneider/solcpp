import { Project } from "ts-morph"
import { CodeBlockWriter } from "ts-morph";

export function generateAccounts(idl) {
    if (idl.accounts === undefined || idl.accounts.length === 0) {
        return
    }
    genAccountFiles(idl)
}

function fieldinterface(acc, writer) {

    writer.write(`class ${acc.name}Fields`).inlineBlock(() => {
        writer.writeLine("public:")
        for (let i = 0; i < acc.type.fields.length; i++) {
            writer.writeLine("std::string " + acc.type.fields[i].name + ";")
        }
    }).write(`;`).newLine();

}

function fieldinterfacejson(acc, writer) {

    writer.write(`class ${acc.name}JSON`).inlineBlock(() => {
        for (let i = 0; i < acc.type.fields.length; i++) {
            writer.writeLine("std::string " + acc.type.fields[i].name + ";")
        }
    }).write(`;`).newLine();

}

function fieldinterfaceclass(acc, writer) {

    writer.write(`class ${acc.name}`).inlineBlock(() => {
        for (let i = 0; i < acc.type.fields.length; i++) {
            writer.writeLine("std::string " + acc.type.fields[i].name + ";")
        }
        writer.blankLine()
        writer.write(`${acc.name} ( ${acc.name}Fields field)`).inlineBlock(() => {
            for (let i = 0; i < acc.type.fields.length; i++) {
                writer.writeLine(`this->${acc.type.fields[i].name} = field.${acc.type.fields[i].name};`)
            }
        }
        ).write(`;`).newLine();
        writer.blankLine()
        writer.write(`${acc.name} fetch (Connection c , PublicKey address)`).inlineBlock(() => {
            writer.writeLine(`int info = c.getAccountInfo(address);`)
            writer.write(`if (info == NULL)`).inlineBlock(() => {
                writer.writeLine(`return NULL;`)
            }
            ).newLine();
            writer.writeLine(`return this->decode(info.data);`)
        }).newLine();

        writer.blankLine()
        writer.write(`${acc.name} fetchMultiple (Connection c , PublicKey[] addresses)`).inlineBlock(() => {
            writer.writeLine(`int[] infos =  c.getMultipleAccountsInfo(addresses);`)
            writer.write(`for (auto info : infos)`).inlineBlock(() => {
                writer.write(`if (info == NULL)`).inlineBlock(() => {
                    writer.writeLine(`return NULL;`)
                }
                ).newLine();
                writer.writeLine(`return this->decode(info.data);`)
            }
            ).newLine();
        }).newLine();

        //decode

        writer.blankLine()
        writer.write(`${acc.name}JSON ToJson`).inlineBlock(() => {
            writer.writeLine(`json r = {`)
            for (let i = 0; i < acc.type.fields.length; i++) {
                writer.writeLine(`${acc.type.fields[i].name} = this.${acc.type.fields[i].name}.toString();`)
            }
            writer.writeLine(`} \n return r;`)
        }).newLine();

        writer.blankLine()
        writer.write(`${acc.name} FromJson (json ${acc.name}JSON)`).inlineBlock(() => {

            writer.writeLine(`${acc.name}Fields fie;`)
            writer.writeLine(`${acc.name} obj(fie);`)
            for (let i = 0; i < acc.type.fields.length; i++) {
                writer.writeLine(`obj.${acc.type.fields[i].name} = ${acc.name}JSON.${acc.type.fields[i].name};`)
            }
            writer.writeLine(`return obj;`)

        }).newLine();

    }).write(`;`).newLine();

}


async function genAccountFiles(
    idl
) {
    const project = new Project()
    idl.accounts?.forEach((acc) => {
        const src = project.createSourceFile(`accounts/${acc.name}.cpp`, " ", {
            overwrite: true,
        })
        const writers = new CodeBlockWriter({
            // optional options
            newLine: "\r\n",         // default: "\n"
            indentNumberOfSpaces: 2, // default: 4
            useTabs: false,          // default: false
            useSingleQuote: true     // default: false
        });

        writers.writeLine(`#include<string>`)
        fieldinterface(acc, writers)
        fieldinterfacejson(acc, writers)
        fieldinterfaceclass(acc, writers);
        src.insertText(0, writer => writer.writeLine(writers.toString()));
    })
    await project.save()
}