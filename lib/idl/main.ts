import { Idl } from "@project-serum/anchor"
import * as fs from "fs"
import * as path from "path"
import { Project } from "ts-morph"
import { program } from "commander"

import { generateAccounts } from "./accounts"
import { generateErrors } from "./errors"
import { generateInstructions } from "./instructions"
import { generateProgramId } from "./programId"
import { generateTypes } from "./types"



function main() {
  program
    .description(
  )
    .argument("idl", "file path")
    .argument("out", "output")
    .option(
      "programid",
      "program ID"
    )
    .parse()


  function outPath(filePath: string) {
    return path.join(out, filePath)
  }

  const idlPath = program.args[0]
  const out = program.args[1]
  const programIdOpt: string | null = program.opts().programId || null

  const pathOrStdin = idlPath === "-" ? 0 : idlPath
  const idl = JSON.parse(fs.readFileSync(pathOrStdin, "utf-8")) as Idl

  const project = new Project()

  console.log("generating programId")
  generateProgramId(project, idl, programIdOpt, outPath)
  console.log("generating errors")
  generateErrors(project, idl, outPath)
  console.log("generating instructions")
  generateInstructions(project, idl, outPath)
  console.log("generating types")
  generateTypes(project, idl, outPath)
  console.log("generating accounts")
  generateAccounts(project, idl, outPath)

  console.log("writing in the  files")
  project.save()
}

