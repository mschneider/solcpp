const program = require("commander");
const anchor = require('@project-serum/anchor');
const solana = require('@solana/web3.js');
const fileSystem = require("fs");


function main() {
  program
    .description("generator to interact with anchor IDL")
    .argument("address", "address of the deployed program")
    .argument("output", " output directory")
    .option(
      "idl",
      "idl file path"
    ).parse()

  const address = program.args[0]
  const output = program.args[1]


  //Fetch IDL
  const connection = new solana.Connection(solana.clusterApiUrl('mainnet-beta'))
  const keypair = anchor.web3.Keypair.generate()
  const wallet = new anchor.Wallet(keypair)
  const provider = new anchor.AnchorProvider(connection, wallet)
  const Program = new solana.PublicKey(address);
  const idl = anchor.Program.fetchIdl(Program, provider);

  //output
  function outPath(filePath) {
    return path.join(output, filePath)
  }

  console.log("generating programId.ts...")
  console.log("generating errors.ts...")
  console.log("generating instructions...")
  console.log("generating types...")
  console.log("generating accounts...")


}

main();
