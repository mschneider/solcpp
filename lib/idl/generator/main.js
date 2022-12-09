import * as fileSystem from "fs"
import * as anchor from "@project-serum/anchor"
import * as solana from "@solana/web3.js"

import { generateAccounts } from "./accounts.js"
import { generateTypes } from "./types.js"
import { program } from "commander"


async function main() {
  program
    .description("generator to interact with anchor IDL")
    .argument("address", "address of the deployed program")
    .argument("output", " output directory")
    .option("-fp, --idlFilePath  [value]", "idl file path")
    .parse()

  const address = program.args[0]
  const output = program.args[1]
  const filepath = program.opts().idlFilePath || null


  //Fetch IDL
  const connection = new solana.Connection(solana.clusterApiUrl('mainnet-beta'))
  const keypair = anchor.web3.Keypair.generate()
  const wallet = new anchor.Wallet(keypair)
  const provider = new anchor.AnchorProvider(connection, wallet)
  const Program = new solana.PublicKey(address);
  const idlFromCluster = await anchor.Program.fetchIdl(Program, provider);

  //IDL From File
  const idlFromFile = filepath ? JSON.parse(fileSystem.readFileSync(filepath, "utf-8")) : null


  //generate classes, methods

  //generate accounts
  console.log("generating accounts")
  generateAccounts(idlFromCluster)
  console.log("generating types")
  generateTypes(idlFromCluster);




  console.log("writing in the  files")

}

main();
