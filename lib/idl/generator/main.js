const program = require("commander");
const anchor = require('@project-serum/anchor');
const solana = require('@solana/web3.js');
const fileSystem = require("fs");


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
  console.log("generating programId.ts...")
  console.log("generating errors.ts...")
  console.log("generating instructions...")
  console.log("generating types...")
  console.log("generating accounts...")


}

main();
